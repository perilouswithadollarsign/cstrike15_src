//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


#include "render_pch.h"
#include "brushbatchrender.h"
#include "icliententity.h"
#include "r_decal.h"
#include "gl_rsurf.h"
#include "tier0/vprof.h"
#include <algorithm>


CBrushBatchRender g_BrushBatchRenderer;

FORCEINLINE void ModulateMaterial( IMaterial *pMaterial, float *pOldColor )
{
	if ( g_bIsBlendingOrModulating )
	{
		pOldColor[3] = pMaterial->GetAlphaModulation( );
		pMaterial->GetColorModulation( &pOldColor[0], &pOldColor[1], &pOldColor[2] );
		pMaterial->AlphaModulate( r_blend );
		pMaterial->ColorModulate( r_colormod[0], r_colormod[1], r_colormod[2] );
	}

}

FORCEINLINE void UnModulateMaterial( IMaterial *pMaterial, float *pOldColor )
{
	if ( g_bIsBlendingOrModulating )
	{
		pMaterial->AlphaModulate( pOldColor[3] );
		pMaterial->ColorModulate( pOldColor[0], pOldColor[1], pOldColor[2] );
	}
}

int __cdecl CBrushBatchRender::SurfaceCmp(const surfacelist_t *s0, const surfacelist_t *s1 )
{
	int sortID0 = MSurf_MaterialSortID( s0->surfID );
	int sortID1 = MSurf_MaterialSortID( s1->surfID );

	return sortID0 - sortID1;
}


// Builds a transrender_t, then executes it's drawing commands
void CBrushBatchRender::DrawTranslucentBrushModel( IMatRenderContext *pRenderContext, model_t *model, IClientEntity *baseentity )
{
	transrender_t render;
	render.pLastBatch = NULL;
	render.pLastNode = NULL;
	render.nodeCount = 0;
	render.surfaceCount = 0;
	render.batchCount = 0;
	render.decalSurfaceCount = 0;
	BuildTransLists_r( render, model, model->brush.pShared->nodes + model->brush.firstnode );
	void *pProxyData = baseentity ? baseentity->GetClientRenderable() : NULL;
	DrawTransLists( pRenderContext, render, pProxyData );
}

void CBrushBatchRender::AddSurfaceToBatch( transrender_t &render, transnode_t *pNode, transbatch_t *pBatch, SurfaceHandle_t surfID )
{
	pBatch->surfaceCount++;
	Assert(render.surfaceCount < MAX_TRANS_SURFACES);

	pBatch->indexCount += (MSurf_VertCount( surfID )-2)*3;
	render.surfaces[render.surfaceCount] = surfID;
	render.surfaceCount++;
	if ( SurfaceHasDecals( surfID ) || MSurf_ShadowDecals( surfID ) != SHADOW_DECAL_HANDLE_INVALID )
	{
		Assert(render.decalSurfaceCount < MAX_TRANS_DECALS);
		pNode->decalSurfaceCount++;
		render.decalSurfaces[render.decalSurfaceCount] = surfID;
		render.decalSurfaceCount++;
	}
}

void CBrushBatchRender::AddTransNode( transrender_t &render )
{
	render.pLastNode = &render.nodes[render.nodeCount];
	render.nodeCount++;
	Assert(render.nodeCount < MAX_TRANS_NODES);
	render.pLastBatch = NULL;
	render.pLastNode->firstBatch = render.batchCount;
	render.pLastNode->firstDecalSurface = render.decalSurfaceCount;
	render.pLastNode->batchCount = 0;
	render.pLastNode->decalSurfaceCount = 0;
}

void CBrushBatchRender::AddTransBatch( transrender_t &render, SurfaceHandle_t surfID )
{
	transbatch_t &batch = render.batches[render.pLastNode->firstBatch + render.pLastNode->batchCount];
	Assert(render.batchCount < MAX_TRANS_BATCHES);
	render.pLastNode->batchCount++;
	render.batchCount++;
	batch.firstSurface = render.surfaceCount;
	batch.surfaceCount = 0;
	batch.pMaterial = MSurf_TexInfo( surfID )->material;
	batch.sortID = MSurf_MaterialSortID( surfID );
	batch.indexCount = 0;
	render.pLastBatch = &batch;
	AddSurfaceToBatch( render, render.pLastNode, &batch, surfID );
}

// build node lists
void CBrushBatchRender::BuildTransLists_r( transrender_t &render, model_t *model, mnode_t *node )
{
	float		dot;

	if (node->contents >= 0)
		return;

	// node is just a decision point, so go down the apropriate sides
	// find which side of the node we are on
	cplane_t *plane = node->plane;
	if ( plane->type <= PLANE_Z )
	{
		dot = modelorg[plane->type] - plane->dist;
	}
	else
	{
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
	}

	int side = (dot >= 0) ? 0 : 1;

	// back side first - translucent surfaces need to render in back to front order
	// to appear correctly.
	BuildTransLists_r(render, model, node->children[!side]);

	// emit all surfaces on node
	CUtlVectorFixed<surfacelist_t, 256> sortList;
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface, model->brush.pShared );
	for ( int i = 0; i < node->numsurfaces; i++, surfID++ )
	{
		// skip opaque surfaces
		if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
		{
			if ( ((MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0) )
			{
				// Backface cull here, so they won't do any more work
				if ( ( side ^ !!(MSurf_Flags( surfID ) & SURFDRAW_PLANEBACK)) )
					continue;
			}

			// If this can be appended to the previous batch, do so
			int sortID = MSurf_MaterialSortID( surfID );
			if ( render.pLastBatch && render.pLastBatch->sortID == sortID )
			{
				AddSurfaceToBatch( render, render.pLastNode, render.pLastBatch, surfID );
			}
			else
			{
				// save it off for sorting, then a later append
				int sortIndex = sortList.AddToTail();
				sortList[sortIndex].surfID = surfID;
			}
		}
	}

	// We've got surfaces on this node that can't be added to the previous node
	if ( sortList.Count() )
	{
		// sort by material
		sortList.Sort( SurfaceCmp );

		// add a new sort group
		AddTransNode( render );
		int lastSortID = -1;
		// now add the optimal number of batches to that group
		for ( int i = 0; i < sortList.Count(); i++ )
		{
			surfID = sortList[i].surfID;
			int sortID = MSurf_MaterialSortID( surfID );
			if ( lastSortID == sortID )
			{
				// can be drawn in a single call with the current list of surfaces, append
				AddSurfaceToBatch( render, render.pLastNode, render.pLastBatch, surfID );
			}
			else
			{
				// requires a break (material/lightmap change).
				AddTransBatch( render, surfID );
				lastSortID = sortID;
			}
		}

		// don't batch across decals or decals will sort incorrectly
		if ( render.pLastNode->decalSurfaceCount )
		{
			render.pLastNode = NULL;
			render.pLastBatch = NULL;
		}
	}

	// front side
	BuildTransLists_r(render, model, node->children[side]);
}

void CBrushBatchRender::DrawTransLists( IMatRenderContext *pRenderContext, transrender_t &render, void *pProxyData )
{
	PIXEVENT( pRenderContext, "DrawTransLists()" );

	bool skipLight = false;
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		skipLight = true;
	}

	float pOldColor[4];


	for ( int i = 0; i < render.nodeCount; i++ )
	{
		int j;
		const transnode_t &node = render.nodes[i];
		for ( j = 0; j < node.batchCount; j++ )
		{
			const transbatch_t &batch = render.batches[node.firstBatch+j];

			CMeshBuilder meshBuilder;
			IMaterial *pMaterial = batch.pMaterial;

			ModulateMaterial( pMaterial, pOldColor );

			if ( !skipLight )
			{
				pRenderContext->BindLightmapPage( materialSortInfoArray[batch.sortID].lightmapPageID );
			}
			pRenderContext->Bind( pMaterial, pProxyData );

			IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[batch.sortID], NULL, NULL );
			meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, batch.indexCount );

			for ( int k = 0; k < batch.surfaceCount; k++ )
			{
				SurfaceHandle_t surfID = render.surfaces[batch.firstSurface + k];
				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
				BuildIndicesForSurface( meshBuilder, surfID );

			}
			meshBuilder.End( false, true );

			// Don't leave the material in a bogus state
			UnModulateMaterial( pMaterial, pOldColor );
		}

		if ( node.decalSurfaceCount )
		{
			for ( j = 0; j < node.decalSurfaceCount; j++ )
			{
				SurfaceHandle_t surfID = render.decalSurfaces[node.firstDecalSurface + j];
				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
				if( SurfaceHasDecals( surfID ) )
				{
					DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
				}

				// Add shadows too....
				ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
				if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
				{
					g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
				}
			}

			// Now draw the decals + shadows for this node
			// This order relative to the surfaces is important for translucency to work correctly.
			DecalSurfaceDraw(pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP);

			// FIXME: Decals are not being rendered while illuminated by the flashlight

			// FIXME: Need to draw decals in flashlight rendering mode
			// Retire decals on opaque world surfaces
			R_DecalFlushDestroyList();

			DecalSurfacesInit( true );

			// Draw all shadows on the brush
			g_pShadowMgr->RenderProjectedTextures( pRenderContext );
		}
		if ( g_ShaderDebug.anydebug )
		{
			CUtlVector<msurface2_t *> brushList;
			for ( j = 0; j < node.batchCount; j++ )
			{
				const transbatch_t &batch = render.batches[node.firstBatch+j];
				for ( int k = 0; k < batch.surfaceCount; k++ )
				{
					brushList.AddToTail( render.surfaces[batch.firstSurface + k] );
				}
			}
			DrawDebugInformation( pRenderContext, brushList.Base(), brushList.Count() );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: This is used when the mat_dxlevel is changed to reset the brush
//          models.
//-----------------------------------------------------------------------------
void R_BrushBatchInit( void )
{
	g_BrushBatchRenderer.LevelInit();
}

void CBrushBatchRender::LevelInit()
{
	AUTO_LOCK( m_Mutex );

	unsigned short iNext;
	for( unsigned short i=m_renderList.Head(); i != m_renderList.InvalidIndex(); i=iNext )
	{
		iNext = m_renderList.Next(i);
		delete m_renderList.Element(i);
	}

	m_renderList.Purge();
	ClearRenderHandles();
}

void CBrushBatchRender::ClearRenderHandles( void )
{
	for ( int iBrush = 1 ; iBrush < host_state.worldbrush->numsubmodels ; ++iBrush )
	{
		char szBrushModel[5]; // inline model names "*1", "*2" etc
		Q_snprintf( szBrushModel, sizeof( szBrushModel ), "*%i", iBrush );
		model_t *pModel = modelloader->GetModelForName( szBrushModel, IModelLoader::FMODELLOADER_SERVER );	
		if ( pModel )
		{
			pModel->brush.renderHandle = 0;
		}
	}
}

// Create a compact, optimal list of rendering commands for the opaque parts of a brush model
// NOTE: This just skips translucent surfaces assuming a separate transrender_t pass!
CBrushBatchRender::brushrender_t *CBrushBatchRender::FindOrCreateRenderBatch( model_t *pModel )
{
	if ( !pModel->brush.nummodelsurfaces )
		return NULL;

	AUTO_LOCK( m_Mutex );

	unsigned short index = pModel->brush.renderHandle - 1;
	if ( m_renderList.IsValidIndex( index ) )
		return m_renderList.Element(index);

	brushrender_t *pRender = new brushrender_t;
	index = m_renderList.AddToTail( pRender );
	pModel->brush.renderHandle = index + 1;
	brushrender_t &render = *pRender; 

	render.pPlanes = NULL;
	render.pMeshes = NULL;
	render.planeCount = 0;
	render.meshCount = 0;
	render.totalIndexCount = 0;
	render.totalVertexCount = 0;

	CUtlVector<cplane_t *>	planeList;
	CUtlVector<surfacelist_t> surfaceList;

	int i;

	SurfaceHandle_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
	for (i=0 ; i<pModel->brush.nummodelsurfaces; i++, surfID++)
	{
		// UNDONE: For now, just draw these in a separate pass
		if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
			continue;

#ifndef _PS3
		cplane_t *plane = surfID->plane;
#else
		cplane_t *plane = &surfID->m_plane;
#endif

		int planeIndex = planeList.Find(plane);
		if ( planeIndex == -1 )
		{
			planeIndex = planeList.AddToTail( plane );
		}
		surfacelist_t tmp;
		tmp.surfID = surfID;
		tmp.surfaceIndex = i;
		tmp.planeIndex = planeIndex;
		surfaceList.AddToTail( tmp );
	}
	surfaceList.Sort( SurfaceCmp );
	render.pPlanes = new cplane_t *[planeList.Count()];
	render.planeCount = planeList.Count();
	memcpy( render.pPlanes, planeList.Base(), sizeof(cplane_t *)*planeList.Count() );
	render.pSurfaces = new brushrendersurface_t[surfaceList.Count()];
	render.surfaceCount = surfaceList.Count();

	int meshCount = 0;
	int batchCount = 0;
	int lastSortID = -1;
	IMesh *pLastMesh = NULL;

	brushrendermesh_t *pMesh = NULL;
	brushrendermesh_t tmpMesh[MAX_VERTEX_FORMAT_CHANGES];
	brushrenderbatch_t *pBatch = NULL;
	brushrenderbatch_t tmpBatch[128];

	for ( i = 0; i < surfaceList.Count(); i++ )
	{
		render.pSurfaces[i].surfaceIndex = surfaceList[i].surfaceIndex;
		render.pSurfaces[i].planeIndex = surfaceList[i].planeIndex;

		surfID = surfaceList[i].surfID;
		int sortID = MSurf_MaterialSortID( surfID );
		if ( g_WorldStaticMeshes[sortID] != pLastMesh )
		{
			pMesh = tmpMesh + meshCount;
			pMesh->firstBatch = batchCount;
			pMesh->batchCount = 0;
			lastSortID = -1; // force a new batch
			meshCount++;
		}
		if ( sortID != lastSortID )
		{
			pBatch = tmpBatch + batchCount;
			pBatch->firstSurface = i;
			pBatch->surfaceCount = 0;
			pBatch->sortID = sortID;
			pBatch->pMaterial = MSurf_TexInfo( surfID )->material;
			pBatch->indexCount = 0;
			pMesh->batchCount++;
			batchCount++;
		}
		pLastMesh = g_WorldStaticMeshes[sortID];
		lastSortID = sortID;
		pBatch->surfaceCount++;
		int vertCount = MSurf_VertCount( surfID );
		int indexCount = (vertCount - 2) * 3;
		pBatch->indexCount += indexCount;
		render.totalIndexCount += indexCount;
		render.totalVertexCount += vertCount;
	}

	render.pMeshes = new brushrendermesh_t[meshCount];
	memcpy( render.pMeshes, tmpMesh, sizeof(brushrendermesh_t) * meshCount );
	render.meshCount = meshCount;
	render.pBatches = new brushrenderbatch_t[batchCount];
	memcpy( render.pBatches, tmpBatch, sizeof(brushrenderbatch_t) * batchCount );
	render.batchCount = batchCount;
	return &render;
}


//-----------------------------------------------------------------------------
// Draws an opaque (parts of a) brush model
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawOpaqueBrushModel( IMatRenderContext *pRenderContext, IClientEntity *baseentity, model_t *model, ERenderDepthMode_t DepthMode )
{
	VPROF( "R_DrawOpaqueBrushModel" );
	SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );

	brushrender_t *pRender = FindOrCreateRenderBatch( model );
	int i;
	if ( !pRender )
		return;

	bool skipLight = false;

	PIXEVENT( pRenderContext, "DrawOpaqueBrushModel()" );

	if ( (g_pMaterialSystemConfig->nFullbright == 1) || ( DepthMode == DEPTH_MODE_SHADOW || DepthMode == DEPTH_MODE_SSA0 ) )
	{
		pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		skipLight = true;
	}

	void *pProxyData = baseentity ? baseentity->GetClientRenderable() : NULL;
	int backface[1024];
	Assert( pRender->planeCount < 1024 );

	// NOTE: Backface culling is almost no perf gain.  Can be removed from brush model rendering.
	// Check the shared planes once
	if ( DepthMode == DEPTH_MODE_SHADOW || DepthMode == DEPTH_MODE_SSA0 )
	{
		for ( i = 0; i < pRender->planeCount; i++ )
		{
			backface[i] = 0;
		}
	}
	else
	{
		for ( i = 0; i < pRender->planeCount; i++ )
		{
			float dot = DotProduct( modelorg, pRender->pPlanes[i]->normal) - pRender->pPlanes[i]->dist;
			backface[i] = ( dot < BACKFACE_EPSILON ) ? true : false; // don't backface cull when rendering to shadow map
		}
	}

	float pOldColor[4];

	// PORTAL 2 PAINT RENDERING
	CUtlVectorFixedGrowable< SurfaceHandle_t, 64 > paintableSurfaces;
	CUtlVectorFixedGrowable< int, 16 > batchPaintableSurfaceCount;
	CUtlVectorFixedGrowable< int, 16 > batchPaintableSurfaceIndexCount;

	for ( i = 0; i < pRender->meshCount; i++ )
	{
		brushrendermesh_t &mesh = pRender->pMeshes[i];
		for ( int j = 0; j < mesh.batchCount; j++ )
		{
			int nBatchIndex = batchPaintableSurfaceCount.Count();
			batchPaintableSurfaceCount.AddToTail( 0 );
			batchPaintableSurfaceIndexCount.AddToTail( 0 );

			brushrenderbatch_t &batch = pRender->pBatches[mesh.firstBatch + j];

			int k;
			for ( k = 0; k < batch.surfaceCount; k++ )
			{
				brushrendersurface_t &surface = pRender->pSurfaces[batch.firstSurface + k];
				if ( !backface[surface.planeIndex] )
					break;
			}

			if ( k == batch.surfaceCount )
				continue;

			CMeshBuilder meshBuilder;
			IMaterial *pMaterial = NULL;

			if ( DepthMode != DEPTH_MODE_NORMAL )
			{
				// Select proper override material
				int nAlphaTest = (int) batch.pMaterial->IsAlphaTested();
				int nNoCull = (int) batch.pMaterial->IsTwoSided();
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
					IMaterialVar *pOriginalTextureVar = batch.pMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
					static unsigned int originalTextureFrameVarCache = 0;
					IMaterialVar *pOriginalTextureFrameVar = batch.pMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
					static unsigned int originalAlphaRefCache = 0;
					IMaterialVar *pOriginalAlphaRefVar = batch.pMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

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

				pMaterial = pDepthWriteMaterial;
			}
			else
			{
				pMaterial = batch.pMaterial;

				// Store off the old color + alpha
				ModulateMaterial( pMaterial, pOldColor );
				if ( !skipLight )
				{
					pRenderContext->BindLightmapPage( materialSortInfoArray[batch.sortID].lightmapPageID );
				}
			}

			pRenderContext->Bind( pMaterial, pProxyData );
			IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[batch.sortID], NULL, NULL );
			meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, batch.indexCount );

			for ( ; k < batch.surfaceCount; k++ )
			{
				brushrendersurface_t &surface = pRender->pSurfaces[batch.firstSurface + k];
				if ( backface[surface.planeIndex] )
					continue;
				SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;

				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

				// PORTAL 2 PAINT RENDERING
				if ( (DepthMode == DEPTH_MODE_NORMAL) && ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED ) )
				{
					paintableSurfaces.AddToTail( surfID );
					++ batchPaintableSurfaceCount[ nBatchIndex ];

					int nVertCount, nIndexCount;

					Shader_GetSurfVertexAndIndexCount( surfID, &nVertCount, &nIndexCount );
					batchPaintableSurfaceIndexCount[ nBatchIndex ] += nIndexCount;
				}
				BuildIndicesForSurface( meshBuilder, surfID );

				if( SurfaceHasDecals( surfID ) && (DepthMode == DEPTH_MODE_NORMAL))
				{
					DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
				}

				// Add overlay fragments to list.
				// FIXME: A little code support is necessary to get overlays working on brush models
				//	OverlayMgr()->AddFragmentListToRenderList( MSurf_OverlayFragmentList( surfID ), false );

				if ( DepthMode == DEPTH_MODE_NORMAL )
				{
					// Add render-to-texture shadows too....
					ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
					if (decalHandle != SHADOW_DECAL_HANDLE_INVALID)
					{
						g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
					}
				}
			}

			meshBuilder.End( false, true );

			if ( DepthMode == DEPTH_MODE_NORMAL )
			{
				// Don't leave the material in a bogus state
				UnModulateMaterial( pMaterial, pOldColor );
			}
		}
	}

	if ( DepthMode != DEPTH_MODE_NORMAL )
		return;

	// PORTAL 2 PAINT RENDERING
	if ( paintableSurfaces.Count() )
	{
		pRenderContext->SetRenderingPaint( true );
		PIXEVENT( pRenderContext, "Paint" );

		int nBatchIndex = 0;
		int nSurfaceIndex = 0;
		for ( i = 0; i < pRender->meshCount; i++ )
		{
			brushrendermesh_t &mesh = pRender->pMeshes[i];
			for ( int j = 0; j < mesh.batchCount; j++ )
			{
				int nSurfaceCount = batchPaintableSurfaceCount[ nBatchIndex ];
				if ( nSurfaceCount > 0 )
				{
					brushrenderbatch_t &batch = pRender->pBatches[mesh.firstBatch + j];

					IMaterial *pMaterial = batch.pMaterial;

					if ( !skipLight )
					{
						pRenderContext->BindLightmapPage( materialSortInfoArray[batch.sortID].lightmapPageID );
					}

					pRenderContext->Bind( pMaterial, pProxyData );

					CMeshBuilder meshBuilder;
					IMesh *pBuildMesh = pRenderContext->GetDynamicMesh( false, g_WorldStaticMeshes[batch.sortID], NULL, NULL );
					meshBuilder.Begin( pBuildMesh, MATERIAL_TRIANGLES, 0, batchPaintableSurfaceIndexCount[ nBatchIndex ] );

					for ( int i = 0; i < nSurfaceCount; ++ i, ++ nSurfaceIndex )
					{
						BuildIndicesForSurface( meshBuilder, paintableSurfaces[ nSurfaceIndex ] );
					}

					meshBuilder.End( false, true );
				}

				++ nBatchIndex;
			}
		}
		pRenderContext->SetRenderingPaint( false );
	}

	if ( g_ShaderDebug.anydebug )
	{
		for ( i = 0; i < pRender->meshCount; i++ )
		{
			brushrendermesh_t &mesh = pRender->pMeshes[i];
			CUtlVector<msurface2_t *> brushList;
			for ( int j = 0; j < mesh.batchCount; j++ )
			{
				brushrenderbatch_t &batch = pRender->pBatches[mesh.firstBatch + j];
				for ( int k = 0; k < batch.surfaceCount; k++ )
				{
					brushrendersurface_t &surface = pRender->pSurfaces[batch.firstSurface + k];
					if ( backface[surface.planeIndex] )
						continue;
					SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;
					brushList.AddToTail(surfID);
				}
			}
			// now draw debug for each drawn surface
			DrawDebugInformation( pRenderContext, brushList.Base(), brushList.Count() );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws an translucent (sorted) brush model
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawTranslucentBrushModel( IMatRenderContext *pRenderContext, IClientEntity *baseentity, model_t *model, ERenderDepthMode_t DepthMode, bool bDrawOpaque, bool bDrawTranslucent )
{
	if ( bDrawOpaque )
	{
		DrawOpaqueBrushModel( pRenderContext, baseentity, model, DepthMode );
	}

	if ( ( DepthMode == DEPTH_MODE_NORMAL ) && bDrawTranslucent )
	{
		DrawTranslucentBrushModel( pRenderContext, model, baseentity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a brush model shadow for render-to-texture shadows
//-----------------------------------------------------------------------------
// UNDONE: This is reasonable, but it could be much faster as follows:
// Build a vertex buffer cache.  A block-allocated static mesh with 1024 verts
// per block or something.
// When a new brush is encountered, fill it in to the current block or the 
// next one (first fit allocator).  Then this routine could simply draw
// a static mesh with a single index buffer build, draw call (no dynamic vb).
void CBrushBatchRender::DrawBrushModelShadow( IMatRenderContext *pRenderContext, model_t *model, IClientRenderable *pRenderable )
{
	brushrender_t *pRender = FindOrCreateRenderBatch( (model_t *)model );
	if ( !pRender )
		return;

	pRenderContext->Bind( g_pMaterialShadowBuild, pRenderable );

	// Draws all surfaces in the brush model in arbitrary order
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( model->brush.firstmodelsurface, model->brush.pShared );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, pRender->totalVertexCount, pRender->totalIndexCount );

	for ( int i=0 ; i<model->brush.nummodelsurfaces ; i++, surfID++)
	{
		Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );

		if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
			continue;

		int startVert = MSurf_FirstVertIndex( surfID );
		int vertCount = MSurf_VertCount( surfID );
		int startIndex = meshBuilder.GetCurrentVertex();
		int j;
		for ( j = 0; j < vertCount; j++ )
		{
			int vertIndex = model->brush.pShared->vertindices[startVert + j];

			// world-space vertex
			meshBuilder.Position3fv( model->brush.pShared->vertexes[vertIndex].position.Base() );
			meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
			meshBuilder.AdvanceVertex();
		}

		for ( j = 0; j < vertCount-2; j++ )
		{
			meshBuilder.FastIndex( startIndex );
			meshBuilder.FastIndex( startIndex + j + 1 );
			meshBuilder.FastIndex( startIndex + j + 2 );
		}
	}
	meshBuilder.End();
	pMesh->Draw();
}



inline bool __cdecl CBrushBatchRender::BatchSortLessFunc( const BrushBatchRenderData_t &left, const BrushBatchRenderData_t &right )
{
	brushrenderbatch_t &leftBatch = left.m_pBrushRender->pBatches[ left.m_nBatchIndex ];
	brushrenderbatch_t &rightBatch = right.m_pBrushRender->pBatches[ right.m_nBatchIndex ];

	if ( left.m_pMaterial != right.m_pMaterial )
		return left.m_pMaterial < right.m_pMaterial;
	if ( leftBatch.sortID != rightBatch.sortID )
		return leftBatch.sortID < rightBatch.sortID;
	if ( left.m_pInstanceData->m_pStencilState != right.m_pInstanceData->m_pStencilState )
		return left.m_pInstanceData->m_pStencilState < right.m_pInstanceData->m_pStencilState;
	if ( left.m_pInstanceData->m_pBrushToWorld != right.m_pInstanceData->m_pBrushToWorld )
		return left.m_pInstanceData->m_pBrushToWorld < right.m_pInstanceData->m_pBrushToWorld;
	return false;
}


//-----------------------------------------------------------------------------
// Builds the lists of stuff to draw
//-----------------------------------------------------------------------------
void CBrushBatchRender::BuildBatchListToDraw( int nCount, const BrushArrayInstanceData_t *pInstanceData, 
	CUtlVectorFixedGrowable< BrushBatchRenderData_t, 1024 > &batchesToRender, brushrender_t **ppBrushRender )
{
	for ( int i = 0; i < nCount; ++i )
	{
		const BrushArrayInstanceData_t &instance = pInstanceData[i];
		brushrender_t *pRender = g_BrushBatchRenderer.FindOrCreateRenderBatch( const_cast< model_t* >( instance.m_pBrushModel ) );
		ppBrushRender[i] = pRender;
		if ( !pRender )
			continue;

		for ( int m = 0; m < pRender->meshCount; ++m )
		{
			brushrendermesh_t &mesh = pRender->pMeshes[m];
			int nBatchIndex = mesh.firstBatch;
			for ( int j = 0; j < mesh.batchCount; ++j, ++nBatchIndex )
			{
				int nIndex = batchesToRender.AddToTail();
				BrushBatchRenderData_t &batch = batchesToRender[nIndex];
				batch.m_pMaterial = pRender->pBatches[nBatchIndex].pMaterial;
				batch.m_pInstanceData = &instance;
				batch.m_pBrushRender = pRender;
				batch.m_nBatchIndex = nBatchIndex;
				Assert( nBatchIndex < ( 1 << 15 ) );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Computes lightmap pages
//-----------------------------------------------------------------------------
void CBrushBatchRender::ComputeLightmapPages( int nCount, BrushBatchRenderData_t *pRenderData )
{
	if ( g_pMaterialSystemConfig->nFullbright != 1 )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			brushrenderbatch_t &batch = pRenderData[i].m_pBrushRender->pBatches[ pRenderData[i].m_nBatchIndex ];
			int nSortID = batch.sortID;
			Assert( nSortID >= 0 && nSortID < g_WorldStaticMeshes.Count() );
			pRenderData[i].m_nLightmapPage = materialSortInfoArray[ nSortID ].lightmapPageID;
		}
		return;
	}

	// Fullbright case. Bump works for unbumped surfaces too
	for ( int i = 0; i < nCount; ++i )
	{
		pRenderData[i].m_nLightmapPage = MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP;
	}
}


//-----------------------------------------------------------------------------
// Computes the # of indices in an instance group
//-----------------------------------------------------------------------------
int CBrushBatchRender::ComputeInstanceGroups( IMatRenderContext *pRenderContext, int nCount, BrushBatchRenderData_t *pRenderData, CUtlVectorFixedGrowable< BrushInstanceGroup_t, 512 > &instanceGroups )
{
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	int nMaxInstanceCount = 0;
	IMaterial *pLastMaterial = NULL;
	IMaterial *pLastActualMaterial = NULL;
	BrushBatchRenderData_t *pFirstInstance = NULL;
	int nInstanceCount = 0;
	int nIndexCount = 0;
	for ( int i = 0; i < nCount; i++ )
	{
		BrushBatchRenderData_t &renderData = pRenderData[i];
		brushrenderbatch_t &batch = renderData.m_pBrushRender->pBatches[ renderData.m_nBatchIndex ];

		int nNextIndexCount = nIndexCount + batch.indexCount;

		// Fire it away if the material changes	or we overflow index count
		if ( ( pLastMaterial != batch.pMaterial ) || ( pLastMaterial != pLastActualMaterial ) || ( nNextIndexCount > nMaxIndices ) )
		{
			if ( nInstanceCount > 0 )
			{
				int nIndex = instanceGroups.AddToTail();
				instanceGroups[nIndex].m_pRenderData = pFirstInstance;
				instanceGroups[nIndex].m_nCount = nInstanceCount;
				instanceGroups[nIndex].m_nIndexCount = nIndexCount;
				instanceGroups[nIndex].m_pMaterial = pLastMaterial;
				instanceGroups[nIndex].m_pActualMaterial = pLastActualMaterial;
				if ( nInstanceCount > nMaxInstanceCount )
				{
					nMaxInstanceCount = nInstanceCount;
				}
			}
			nInstanceCount = 0;
			pFirstInstance = &renderData;
			nIndexCount = batch.indexCount;
			pLastMaterial = renderData.m_pMaterial;

			// This is necessary for shadow depth rendering. We need to re-bind
			// for every alpha tested material
			pLastActualMaterial = renderData.m_nIsAlphaTested ? batch.pMaterial : pLastMaterial;
		}
		else
		{
			nIndexCount = nNextIndexCount;
		}

		++nInstanceCount;
	}

	if ( nInstanceCount > 0 )
	{
		int nIndex = instanceGroups.AddToTail();
		instanceGroups[nIndex].m_pRenderData = pFirstInstance;
		instanceGroups[nIndex].m_nCount = nInstanceCount;
		instanceGroups[nIndex].m_nIndexCount = nIndexCount;
		instanceGroups[nIndex].m_pMaterial = pLastMaterial;
		instanceGroups[nIndex].m_pActualMaterial = pLastActualMaterial;
		if ( nInstanceCount > nMaxInstanceCount )
		{
			nMaxInstanceCount = nInstanceCount;
		}
	}

	return nMaxInstanceCount;
}


//-----------------------------------------------------------------------------
// Draws an opaque (parts of a) brush model
//-----------------------------------------------------------------------------
bool CBrushBatchRender::DrawSortedBatchList( IMatRenderContext* pRenderContext, int nCount, BrushInstanceGroup_t *pInstanceGroup, int nMaxInstanceCount )
{
	VPROF( "DrawSortedBatchList" );
	PIXEVENT( pRenderContext, "DrawSortedBatchList()" );

	// Needed to allow the system to detect which samplers are bound to lightmap textures
	pRenderContext->BindLightmapPage( 0 );

	MeshInstanceData_t *pInstance = (MeshInstanceData_t*)stackalloc( nMaxInstanceCount * sizeof(MeshInstanceData_t) );

	bool bHasPaintedSurfaces = false;
	for ( int i = 0; i < nCount; i++ )
	{
		BrushInstanceGroup_t &group = pInstanceGroup[i];

		pRenderContext->Bind( group.m_pMaterial, NULL );

		// Only writing indices
		// FIXME: Can we make this a static index buffer?
		IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer();
		CIndexBuilder indexBuilder( pBuildIndexBuffer, MATERIAL_INDEX_FORMAT_16BIT );
		indexBuilder.Lock( group.m_nIndexCount, 0 );
		int nIndexOffset = indexBuilder.Offset() / sizeof(uint16);

		group.m_nHasPaintedSurfaces = false;
		for ( int j = 0; j < group.m_nCount; ++j )
		{
			BrushBatchRenderData_t &renderData = group.m_pRenderData[j];
			const brushrenderbatch_t &batch = renderData.m_pBrushRender->pBatches[ renderData.m_nBatchIndex ];
			renderData.m_nHasPaintedSurfaces = false;
			const model_t *pModel = renderData.m_pInstanceData->m_pBrushModel;
			SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
			for ( int k = 0; k < batch.surfaceCount; k++ )
			{
				const brushrendersurface_t &surface = renderData.m_pBrushRender->pSurfaces[batch.firstSurface + k];
				SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;

				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
				BuildIndicesForSurface( indexBuilder, surfID );
				if ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED )
				{
					group.m_nHasPaintedSurfaces = true;
					renderData.m_nHasPaintedSurfaces = true;
					bHasPaintedSurfaces = true;
				}
			}

			MeshInstanceData_t &instance = pInstance[ j ];
			instance.m_pEnvCubemap = NULL;
			instance.m_pPoseToWorld = renderData.m_pInstanceData->m_pBrushToWorld;
			instance.m_pLightingState = NULL;
			instance.m_nBoneCount = 1;
			instance.m_pBoneRemap = NULL;
			instance.m_nIndexOffset = nIndexOffset;
			instance.m_nIndexCount = batch.indexCount;
			instance.m_nPrimType = MATERIAL_TRIANGLES;
			instance.m_pColorBuffer = NULL;
			instance.m_nColorVertexOffsetInBytes = 0;
			instance.m_pStencilState = renderData.m_pInstanceData->m_pStencilState;
			instance.m_pVertexBuffer = g_WorldStaticMeshes[ batch.sortID ];
			instance.m_pIndexBuffer = pBuildIndexBuffer;
			instance.m_nVertexOffsetInBytes = 0;
			instance.m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
			instance.m_nLightmapPageId = renderData.m_nLightmapPage;
			instance.m_bColorBufferHasIndirectLightingOnly = false;

			nIndexOffset += batch.indexCount;
		}

		indexBuilder.End( );
		pRenderContext->DrawInstances( group.m_nCount, pInstance );
	}

	return bHasPaintedSurfaces;
}


void CBrushBatchRender::DrawPaintForBatches( IMatRenderContext* pRenderContext, int nCount, const BrushInstanceGroup_t *pInstanceGroup, int nMaxInstanceCount )
{
	MeshInstanceData_t *pInstance = (MeshInstanceData_t*)stackalloc( nMaxInstanceCount * sizeof(MeshInstanceData_t) );

	pRenderContext->SetRenderingPaint( true );
	PIXEVENT( pRenderContext, "Paint" );

	for ( int i = 0; i < nCount; i++ )
	{
		const BrushInstanceGroup_t &group = pInstanceGroup[i];
		if ( !group.m_nHasPaintedSurfaces )
			continue;

		pRenderContext->Bind( group.m_pMaterial, NULL );

		// Only writing indices, we're potentially allocating too many, but that's ok.
		// Unused ones will be freed up
		// FIXME: Can we make this a static index buffer?
		IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer();
		CIndexBuilder indexBuilder( pBuildIndexBuffer, MATERIAL_INDEX_FORMAT_16BIT );
		indexBuilder.Lock( group.m_nIndexCount, 0 );
		int nIndexOffset = indexBuilder.Offset() / sizeof(uint16);
		int nGroupCount = 0;
		for ( int j = 0; j < group.m_nCount; ++j )
		{
			const BrushBatchRenderData_t &renderData = group.m_pRenderData[j];
			if ( !renderData.m_nHasPaintedSurfaces )
				continue;

			const brushrenderbatch_t &batch = renderData.m_pBrushRender->pBatches[ renderData.m_nBatchIndex ];
			const model_t *pModel = renderData.m_pInstanceData->m_pBrushModel;
			SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
			int nBatchIndexCount = 0;
			for ( int k = 0; k < batch.surfaceCount; k++ )
			{
				const brushrendersurface_t &surface = renderData.m_pBrushRender->pSurfaces[batch.firstSurface + k];
				SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;
				if ( ( MSurf_Flags( surfID ) & SURFDRAW_PAINTED ) == 0 )
					continue;

				int nTriCount = BuildIndicesForSurface( indexBuilder, surfID );
				nBatchIndexCount += nTriCount * 3;
			}

			MeshInstanceData_t &instance = pInstance[ nGroupCount ];
			instance.m_pEnvCubemap = NULL;
			instance.m_pPoseToWorld = renderData.m_pInstanceData->m_pBrushToWorld;
			instance.m_pLightingState = NULL;
			instance.m_nBoneCount = 1;
			instance.m_pBoneRemap = NULL;
			instance.m_nIndexOffset = nIndexOffset;
			instance.m_nIndexCount = nBatchIndexCount;
			instance.m_nPrimType = MATERIAL_TRIANGLES;
			instance.m_pColorBuffer = NULL;
			instance.m_nColorVertexOffsetInBytes = 0;
			instance.m_pStencilState = renderData.m_pInstanceData->m_pStencilState;
			instance.m_pVertexBuffer = g_WorldStaticMeshes[ batch.sortID ];
			instance.m_pIndexBuffer = pBuildIndexBuffer;
			instance.m_nVertexOffsetInBytes = 0;
			instance.m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
			instance.m_nLightmapPageId = renderData.m_nLightmapPage;
			instance.m_bColorBufferHasIndirectLightingOnly = false;

			nIndexOffset += nBatchIndexCount;
			++nGroupCount;
		}

		indexBuilder.End( );
		pRenderContext->DrawInstances( nGroupCount, pInstance );
	}

	pRenderContext->SetRenderingPaint( false );
}


// Draw decals
void CBrushBatchRender::DrawDecalsForBatches( IMatRenderContext *pRenderContext, 
	int nCount, const BrushArrayInstanceData_t *pInstanceData, brushrender_t **ppBrushRender )
{
	// FIXME: This could be better optimized by rendering across instances
	// but that's a deeper change: we'd need to have per-instance transforms
	// for each decal + shadow
	for ( int i = 0; i < nCount; ++i )
	{
		// Clear out the render list of decals
		DecalSurfacesInit( true );

		// Clear out the render lists of shadows
		g_pShadowMgr->ClearShadowRenderList( );

		const BrushArrayInstanceData_t &instance = pInstanceData[i];
		const brushrender_t *pRender = ppBrushRender[i];
		if ( !pRender )
			continue;

		SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( instance.m_pBrushModel->brush.firstmodelsurface, instance.m_pBrushModel->brush.pShared );

		bool bEncounteredDecals = false;
		for ( int s = 0; s < pRender->surfaceCount; ++s )
		{
			const brushrendersurface_t &surface = pRender->pSurfaces[s];
			SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;

			if ( SurfaceHasDecals( surfID ) )
			{
				bEncounteredDecals = true;
				DecalSurfaceAdd( surfID, BRUSHMODEL_DECAL_SORT_GROUP );
			}

			// Add overlay fragments to list.
			// FIXME: A little code support is necessary to get overlays working on brush models
			//	OverlayMgr()->AddFragmentListToRenderList( MSurf_OverlayFragmentList( surfID ), false );

			// Add render-to-texture shadows too....
			ShadowDecalHandle_t decalHandle = MSurf_ShadowDecals( surfID );
			if ( decalHandle != SHADOW_DECAL_HANDLE_INVALID )
			{
				bEncounteredDecals = true;
				g_pShadowMgr->AddShadowsOnSurfaceToRenderList( decalHandle );
			}
		}

		if ( bEncounteredDecals )
		{
			CBrushModelTransform pushTransform( *instance.m_pBrushToWorld, pRenderContext );

			// Draw all shadows on the brush
			g_pShadowMgr->RenderProjectedTextures( pRenderContext );

			DecalSurfaceDraw( pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP, instance.m_DiffuseModulation.w );

			// draw the flashlight lighting for the decals on the brush.
			g_pShadowMgr->DrawFlashlightDecals( pRenderContext, BRUSHMODEL_DECAL_SORT_GROUP, false, instance.m_DiffuseModulation.w );

			// Retire decals on opaque brushmodel surfaces
			R_DecalFlushDestroyList();

			
		}
	}
}


void CBrushBatchRender::DrawArrayDebugInformation( IMatRenderContext *pRenderContext, int nCount, const BrushBatchRenderData_t *pRenderData )
{
	if ( !g_ShaderDebug.anydebug )
		return;

	const Vector &vecViewOrigin = g_EngineRenderer->ViewOrigin();

	for ( int r = 0; r < nCount; ++r )
	{
		const BrushBatchRenderData_t &renderData = pRenderData[r];
		const brushrenderbatch_t &batch = renderData.m_pBrushRender->pBatches[ renderData.m_nBatchIndex ];
		const model_t *pModel = renderData.m_pInstanceData->m_pBrushModel;
		SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );

		Vector vecModelSpaceViewOrigin;
		VectorITransform( vecViewOrigin, *renderData.m_pInstanceData->m_pBrushToWorld, vecModelSpaceViewOrigin ); 

		CUtlVectorFixedGrowable< msurface2_t *, 512 > surfaceList;
		for ( int k = 0; k < batch.surfaceCount; k++ )
		{
			brushrendersurface_t &surface = renderData.m_pBrushRender->pSurfaces[batch.firstSurface + k];
			SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;

			if ( MSurf_Flags(surfID) & SURFDRAW_TRANS )
				continue;

			if ( ( MSurf_Flags( surfID ) & SURFDRAW_NOCULL) == 0 )
			{
				// Do a full backface cull here; we're not culling elsewhere in the pipeline
				// Yes, it's expensive, but this is a debug mode, so who cares?
				cplane_t *pSurfacePlane = renderData.m_pBrushRender->pPlanes[surface.planeIndex];
				float flDot = DotProduct( vecModelSpaceViewOrigin, pSurfacePlane->normal ) - pSurfacePlane->dist;
				bool bIsBackfacing = ( flDot < BACKFACE_EPSILON ) ? true : false;
				if ( bIsBackfacing != false )
					continue;
			}

			surfaceList.AddToTail( surfID );
		}

		// now draw debug for each drawn surface
		DrawDebugInformation( pRenderContext, *renderData.m_pInstanceData->m_pBrushToWorld, surfaceList.Base(), surfaceList.Count() );
	}
}


//-----------------------------------------------------------------------------
// Main entry point for rendering an array of brush models
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawBrushModelArray( IMatRenderContext* pRenderContext, int nCount, 
	const BrushArrayInstanceData_t *pInstanceData )
{
	brushrender_t **ppBrushRender = (brushrender_t**)stackalloc( nCount * sizeof(brushrender_t*) );
	CUtlVectorFixedGrowable< BrushBatchRenderData_t, 1024 > batchesToRender;
	BuildBatchListToDraw( nCount, pInstanceData, batchesToRender, ppBrushRender );

	int nBatchCount = batchesToRender.Count();
	BrushBatchRenderData_t *pBatchData = batchesToRender.Base();

	ComputeLightmapPages( nBatchCount, pBatchData );

	std::make_heap( pBatchData, pBatchData + nBatchCount, BatchSortLessFunc ); 
	std::sort_heap( pBatchData, pBatchData + nBatchCount, BatchSortLessFunc );

	CUtlVectorFixedGrowable< BrushInstanceGroup_t, 512 > instanceGroups;
	int nMaxInstanceCount = ComputeInstanceGroups( pRenderContext, nBatchCount, pBatchData, instanceGroups );

	bool bHasPaintedSurfaces = DrawSortedBatchList( pRenderContext, instanceGroups.Count(), instanceGroups.Base(), nMaxInstanceCount );
	if ( bHasPaintedSurfaces )
	{
		DrawPaintForBatches( pRenderContext, instanceGroups.Count(), instanceGroups.Base(), nMaxInstanceCount );
	}

	DrawDecalsForBatches( pRenderContext, nCount, pInstanceData, ppBrushRender );

	DrawArrayDebugInformation( pRenderContext, nBatchCount, pBatchData );
}


//-----------------------------------------------------------------------------
// Builds the lists of shadow batches to draw
//-----------------------------------------------------------------------------
void CBrushBatchRender::BuildShadowBatchListToDraw( int nCount, const BrushArrayInstanceData_t *pInstanceData, 
	CUtlVectorFixedGrowable< BrushBatchRenderData_t, 1024 > &batchesToRender, int  nModelTypeFlags )
{
	for ( int i = 0; i < nCount; ++i )
	{
		const BrushArrayInstanceData_t &instance = pInstanceData[i];
		brushrender_t *pRender = g_BrushBatchRenderer.FindOrCreateRenderBatch( const_cast< model_t* >( instance.m_pBrushModel ) );
		if ( !pRender )
			continue;

		for ( int m = 0; m < pRender->meshCount; ++m )
		{
			brushrendermesh_t &mesh = pRender->pMeshes[m];
			int nBatchIndex = mesh.firstBatch;
			for ( int j = 0; j < mesh.batchCount; ++j, ++nBatchIndex )
			{
				// Select proper override material
				const brushrenderbatch_t &renderBatch = pRender->pBatches[ nBatchIndex ];
				int nAlphaTest = (int)renderBatch.pMaterial->IsAlphaTested();
				int nNoCull = (int)renderBatch.pMaterial->IsTwoSided();

				IMaterial *pDepthWriteMaterial;

				if ( nModelTypeFlags & STUDIO_SSAODEPTHTEXTURE )
				{
					pDepthWriteMaterial = g_pMaterialSSAODepthWrite[ nAlphaTest ][ nNoCull ];
				}
				else
				{
					pDepthWriteMaterial = g_pMaterialDepthWrite[ nAlphaTest ][ nNoCull ];
				}

				int nIndex = batchesToRender.AddToTail();
				BrushBatchRenderData_t &batch = batchesToRender[nIndex];
				batch.m_pInstanceData = &instance;
				batch.m_pBrushRender = pRender;
				batch.m_nBatchIndex = nBatchIndex;
				batch.m_nIsAlphaTested = nAlphaTest;
				batch.m_pMaterial = pDepthWriteMaterial;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Sorts shadows
//-----------------------------------------------------------------------------
inline bool __cdecl CBrushBatchRender::ShadowSortLessFunc( const BrushBatchRenderData_t &left, const BrushBatchRenderData_t &right )
{
	if ( left.m_pMaterial != right.m_pMaterial )
		return left.m_pMaterial < right.m_pMaterial;
	if ( left.m_pInstanceData->m_pBrushToWorld != right.m_pInstanceData->m_pBrushToWorld )
		return left.m_pInstanceData->m_pBrushToWorld < right.m_pInstanceData->m_pBrushToWorld;
	return false;
}



//-----------------------------------------------------------------------------
// Draws an opaque (parts of a) brush model
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawShadowBatchList( IMatRenderContext* pRenderContext, int nCount, BrushInstanceGroup_t *pInstanceGroup, int nMaxInstanceCount )
{
	VPROF( "DrawShadowBatchList" );
	PIXEVENT( pRenderContext, "DrawShadowBatchList()" );

	pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );

	MeshInstanceData_t *pInstance = (MeshInstanceData_t*)stackalloc( nMaxInstanceCount * sizeof(MeshInstanceData_t) );

	for ( int i = 0; i < nCount; i++ )
	{
		BrushInstanceGroup_t &group = pInstanceGroup[i];

		if ( group.m_pRenderData->m_nIsAlphaTested )
		{
			static unsigned int originalTextureVarCache = 0;
			IMaterialVar *pOriginalTextureVar = group.m_pActualMaterial->FindVarFast( "$basetexture", &originalTextureVarCache );
			static unsigned int originalTextureFrameVarCache = 0;
			IMaterialVar *pOriginalTextureFrameVar = group.m_pActualMaterial->FindVarFast( "$frame", &originalTextureFrameVarCache );
			static unsigned int originalAlphaRefCache = 0;
			IMaterialVar *pOriginalAlphaRefVar = group.m_pActualMaterial->FindVarFast( "$AlphaTestReference", &originalAlphaRefCache );

			static unsigned int textureVarCache = 0;
			IMaterialVar *pTextureVar = group.m_pMaterial->FindVarFast( "$basetexture", &textureVarCache );
			static unsigned int textureFrameVarCache = 0;
			IMaterialVar *pTextureFrameVar = group.m_pMaterial->FindVarFast( "$frame", &textureFrameVarCache );
			static unsigned int alphaRefCache = 0;
			IMaterialVar *pAlphaRefVar = group.m_pMaterial->FindVarFast( "$AlphaTestReference", &alphaRefCache );

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

		pRenderContext->Bind( group.m_pMaterial, NULL );

		// Only writing indices
		// FIXME: Can we make this a static index buffer?
		IIndexBuffer *pBuildIndexBuffer = pRenderContext->GetDynamicIndexBuffer();
		CIndexBuilder indexBuilder( pBuildIndexBuffer, MATERIAL_INDEX_FORMAT_16BIT );
		indexBuilder.Lock( group.m_nIndexCount, 0 );
		int nIndexOffset = indexBuilder.Offset() / sizeof(uint16);

		for ( int j = 0; j < group.m_nCount; ++j )
		{
			BrushBatchRenderData_t &renderData = group.m_pRenderData[j];
			const brushrenderbatch_t &batch = renderData.m_pBrushRender->pBatches[ renderData.m_nBatchIndex ];
			const model_t *pModel = renderData.m_pInstanceData->m_pBrushModel;
			SurfaceHandle_t firstSurfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface, pModel->brush.pShared );
			for ( int k = 0; k < batch.surfaceCount; k++ )
			{
				const brushrendersurface_t &surface = renderData.m_pBrushRender->pSurfaces[batch.firstSurface + k];
				SurfaceHandle_t surfID = firstSurfID + surface.surfaceIndex;

				Assert( !(MSurf_Flags( surfID ) & SURFDRAW_NODRAW) );
				BuildIndicesForSurface( indexBuilder, surfID );
			}

			MeshInstanceData_t &instance = pInstance[ j ];
			instance.m_pEnvCubemap = NULL;
			instance.m_pPoseToWorld = renderData.m_pInstanceData->m_pBrushToWorld;
			instance.m_pLightingState = NULL;
			instance.m_nBoneCount = 1;
			instance.m_pBoneRemap = NULL;
			instance.m_nIndexOffset = nIndexOffset;
			instance.m_nIndexCount = batch.indexCount;
			instance.m_nPrimType = MATERIAL_TRIANGLES;
			instance.m_pColorBuffer = NULL;
			instance.m_nColorVertexOffsetInBytes = 0;
			instance.m_pStencilState = renderData.m_pInstanceData->m_pStencilState;
			instance.m_pVertexBuffer = g_WorldStaticMeshes[ batch.sortID ];
			instance.m_pIndexBuffer = pBuildIndexBuffer;
			instance.m_nVertexOffsetInBytes = 0;
			instance.m_DiffuseModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
			instance.m_nLightmapPageId = MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP;
			instance.m_bColorBufferHasIndirectLightingOnly = false;

			nIndexOffset += batch.indexCount;
		}

		indexBuilder.End( );
		pRenderContext->DrawInstances( group.m_nCount, pInstance );
	}
}


//-----------------------------------------------------------------------------
// Main entry point for rendering an array of brush model shadows
//-----------------------------------------------------------------------------
void CBrushBatchRender::DrawBrushModelShadowArray( IMatRenderContext* pRenderContext, int nCount, 
	const BrushArrayInstanceData_t *pInstanceData, int nModelTypeFlags )
{
	CUtlVectorFixedGrowable< BrushBatchRenderData_t, 1024 > batchesToRender;
	BuildShadowBatchListToDraw( nCount, pInstanceData, batchesToRender, nModelTypeFlags );

	int nBatchCount = batchesToRender.Count();
	BrushBatchRenderData_t *pBatchData = batchesToRender.Base();

	std::make_heap( pBatchData, pBatchData + nBatchCount, ShadowSortLessFunc ); 
	std::sort_heap( pBatchData, pBatchData + nBatchCount, ShadowSortLessFunc );

	CUtlVectorFixedGrowable< BrushInstanceGroup_t, 512 > instanceGroups;
	int nMaxInstanceCount = ComputeInstanceGroups( pRenderContext, nBatchCount, pBatchData, instanceGroups );

	DrawShadowBatchList( pRenderContext, instanceGroups.Count(), instanceGroups.Base(), nMaxInstanceCount );
}
