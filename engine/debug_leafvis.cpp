//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "render_pch.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include "r_areaportal.h"
#include "coordsize.h"

// Leaf visualization routines

// Draw groups can be enabled/disabled for visualization using a bitfield specified by mat_leafvis_draw_mask;
// e.g. -1 = draw all, bit N set = draw group N, so '5' corresponds to draw group 0 and 2 only since 5 = (1 << 0) + (1 << 2)
enum DrawGroup_t
{
	DG_BASE = 0,
	DG_PVS_VISIBLE_LEAVES = 0,
	DG_FRUSTUM_VISIBLE_LEAVES = 1,
	DG_FRUSTUM = 2,
	DG_PVS_INVISIBLE_LEAVES = 3,
	MAX_DRAW_GROUPS = 4,
};

void CSGFrustum( Frustum_t &frustum );

struct leafvis_t
{
	leafvis_t()
	{
		memset( &m_Colors, 0, sizeof( m_Colors ) );
		leafIndex = 0;
		CCollisionBSPData *pBSP = GetCollisionBSPData();
		if ( pBSP )
		{
			numbrushes = pBSP->numbrushes;
			numentitychars = pBSP->numentitychars;
		}
	}

	bool IsValid()
	{
		CCollisionBSPData *pBSP = GetCollisionBSPData();
		if ( !pBSP || numbrushes != pBSP->numbrushes || numentitychars != pBSP->numentitychars )
			return false;

		return true;
	}

	CUtlVector<Vector>		verts;
	struct Polygon_t
	{
		Polygon_t( int nVertCount, DrawGroup_t drawGroup = DG_BASE ) : 
		m_nVertCount( nVertCount ), 
		m_DrawGroup( drawGroup )
		{
		}

		int m_nVertCount;
		DrawGroup_t m_DrawGroup;
	};

	CUtlVector< Polygon_t >	m_Polygons;
	Vector					m_Colors[MAX_DRAW_GROUPS];
	int						numbrushes;
	int						numentitychars;
	int						leafIndex;
};
const int MAX_LEAF_PVERTS = 128;

// Only allocate this after it is turned on
leafvis_t *g_LeafVis = NULL;

leafvis_t *g_FrustumVis = NULL, *g_ClipVis[4] = {NULL,NULL,NULL,NULL};

static void AddPlaneToList( CUtlVector<cplane_t> &list, const Vector& normal, float dist, int invert )
{
	cplane_t plane;
	plane.dist = invert ? -dist : dist;
	plane.normal = invert ? -normal : normal;

	Vector point = plane.dist * plane.normal;
	for ( int i = 0; i < list.Count(); i++ )
	{
		// same plane, remove or replace
		if ( list[i].normal == plane.normal )
		{
			float d = DotProduct(point, list[i].normal) - list[i].dist;
			if ( d > 0 )
			{
				// new plane is in front of the old one
				list[i].dist = plane.dist;
			}
			// new plane is behind the old one
			return;
		}

	}
	list.AddToTail( plane );
}

static void PlaneList( int leafIndex, model_t *model, CUtlVector<cplane_t> &planeList )
{
	if (!model || !model->brush.pShared || !model->brush.pShared->nodes)
		Sys_Error ("PlaneList: bad model");

	mleaf_t *pLeaf = &model->brush.pShared->leafs[leafIndex];
	mnode_t *pNode = pLeaf->parent;
	mnode_t *pChild = (mnode_t *)pLeaf;
	while (pNode)
	{
		// was the child on the front or back of the plane of this node?
		bool front = (pNode->children[0] == pChild) ? true : false;
		AddPlaneToList( planeList, pNode->plane->normal, pNode->plane->dist, !front );
		pChild = pNode;
		pNode = pNode->parent;
	}
}

Vector CSGInsidePoint( cplane_t *pPlanes, int planeCount )
{
	Vector point = vec3_origin;

	for ( int i = 0; i < planeCount; i++ )
	{
		float d = DotProduct( pPlanes[i].normal, point ) - pPlanes[i].dist;
		if ( d < 0 )
		{
			point -= d * pPlanes[i].normal;
		}
	}
	return point;
}

void TranslatePlaneList( cplane_t *pPlanes, int planeCount, const Vector &offset )
{
	for ( int i = 0; i < planeCount; i++ )
	{
		pPlanes[i].dist += DotProduct( offset, pPlanes[i].normal );
	}
}


void CSGPlaneList( leafvis_t *pVis, CUtlVector<cplane_t> &planeList, DrawGroup_t drawGroup = DG_BASE )
{
	int planeCount = planeList.Count();
	Vector	vertsIn[MAX_LEAF_PVERTS], vertsOut[MAX_LEAF_PVERTS];

	// compute a point inside the volume defined by these planes
	Vector insidePoint = CSGInsidePoint( planeList.Base(), planeList.Count() );
	// move the planes so that the inside point is at the origin 
	// NOTE: This is to maximize precision for the CSG operations
	TranslatePlaneList( planeList.Base(), planeList.Count(), -insidePoint );

	// Build the CSG solid of this leaf given that the planes in the list define a convex solid
	for ( int i = 0; i < planeCount; i++ )
	{
		// Build a big-ass poly in this plane
		int vertCount = PolyFromPlane( vertsIn, planeList[i].normal, planeList[i].dist );	// BaseWindingForPlane()
		
		// Now chop it by every other plane
		int j;
		for ( j = 0; j < planeCount; j++ )
		{
			// don't clip planes with themselves
			if ( i == j )
				continue;

			// Less than a poly left, something's wrong, don't bother with this polygon
			if ( vertCount < 3 )
				continue;

			// Chop the polygon against this plane
			vertCount = ClipPolyToPlane( vertsIn, vertCount, vertsOut, planeList[j].normal, planeList[j].dist );
			
			// Just copy the verts each time, don't bother swapping pointers (efficiency is not a goal here)
			for ( int k = 0; k < vertCount; k++ )
			{
				VectorCopy( vertsOut[k], vertsIn[k] );
			}
		}

		// We've got a polygon here
		if ( vertCount >= 3 )
		{
			// Copy polygon out
			pVis->m_Polygons.AddToTail( leafvis_t::Polygon_t( vertCount, drawGroup ) );
			for ( j = 0; j < vertCount; j++ )
			{
				// move the verts back by the initial translation
				Vector vert = vertsIn[j] + insidePoint;
				pVis->verts.AddToTail( vert );
			}
		}
	}
}

void LeafvisChanged( IConVar *pLeafvisVar, const char *pOld, float flOldValue )
{
	if ( g_LeafVis )
	{
		delete g_LeafVis;
		g_LeafVis = NULL;
	}

	if ( g_FrustumVis )
	{
		delete g_FrustumVis;
		g_FrustumVis = NULL;
	}
}

void AddLeafPortals( leafvis_t *pLeafvis, int leafIndex, DrawGroup_t drawGroup = DG_BASE )
{
	CUtlVector<cplane_t> planeList;
	Vector	normal;

	// Build a list of inward pointing planes of the tree descending to this
	PlaneList( leafIndex, host_state.worldmodel, planeList );
	
	VectorCopy( vec3_origin, normal );
	// Add world bounding box planes in case the world isn't closed
	// x-axis
	normal[0] = 1;
	AddPlaneToList( planeList, normal, MAX_COORD_INTEGER, true );
	AddPlaneToList( planeList, normal, -MAX_COORD_INTEGER, false );
	normal[0] = 0;
	
	// y-axis
	normal[1] = 1;
	AddPlaneToList( planeList, normal, MAX_COORD_INTEGER, true );
	AddPlaneToList( planeList, normal, -MAX_COORD_INTEGER, false );
	normal[1] = 0;
	
	// z-axis
	normal[2] = 1;
	AddPlaneToList( planeList, normal, MAX_COORD_INTEGER, true );
	AddPlaneToList( planeList, normal, -MAX_COORD_INTEGER, false );
	CSGPlaneList( pLeafvis, planeList, drawGroup );
}

ConVar mat_leafvis("mat_leafvis","0", FCVAR_CHEAT, "Draw wireframe of: [0] nothing, [1] current leaf, [2] entire vis cluster, or [3] entire PVS (see mat_leafvis_draw_mask for what does/doesn't get drawn)", LeafvisChanged );
ConVar mat_leafvis_update_every_frame( "mat_leafvis_update_every_frame", "0", 0, "Updates leafvis debug render every frame (expensive)" );
ConVar r_visambient("r_visambient","0", 0, "Draw leaf ambient lighting samples.  Needs mat_leafvis 1 to work" );
ConVar mat_leafvis_draw_mask( "mat_leafvis_draw_mask", "3", 0, "A bitfield which affects leaf visibility debug rendering.  -1: show all, bit 0: render PVS-visible leafs, bit 1: render PVS- and frustum-visible leafs, bit 2: render frustum bounds, bit 3: render leaves out of PVS." );
ConVar mat_leafvis_freeze( "mat_leafvis_freeze", "0", 0, "If set to 1, uses the last known leaf visibility data for visualization.  If set to 0, updates based on camera movement." );

//-----------------------------------------------------------------------------
// Purpose: Builds a convex polyhedron of the leaf boundary around p
// Input  : p - point to classify determining the leaf
//-----------------------------------------------------------------------------
void LeafVisBuild( const Vector &p )
{
	if ( !mat_leafvis.GetInt() )
	{
		Assert( !g_LeafVis );
		return;
	}
	else
	{
		static int last_leaf = -1;

		int leafIndex = CM_PointLeafnum( p );

		if ( mat_leafvis_freeze.GetBool() && last_leaf != -1 && last_leaf < host_state.worldmodel->brush.pShared->numleafs )
		{
			leafIndex = last_leaf;
		}

		if ( g_LeafVis && ( last_leaf == leafIndex ) && !mat_leafvis_update_every_frame.GetBool() )
			return;

		bool bSpewOnLeafChanged = ( last_leaf != leafIndex );
		if ( bSpewOnLeafChanged )
		{
			DevMsg( 1, "Leaf %d, Area %d, Cluster %d\n", leafIndex, CM_LeafArea( leafIndex ), CM_LeafCluster( leafIndex ) );
		}
		last_leaf = leafIndex;

		delete g_LeafVis;
		g_LeafVis = new leafvis_t;

		// Color for leafs in PVS
		g_LeafVis->m_Colors[DG_PVS_VISIBLE_LEAVES].Init( 0.0f, 0.0f, 1.0f );
		// Color for leafs in PVS that pass frustum test
		g_LeafVis->m_Colors[DG_FRUSTUM_VISIBLE_LEAVES].Init( 0.0f, 0.0f, 1.0f );
		
		g_LeafVis->m_Colors[DG_PVS_INVISIBLE_LEAVES].Init( 0.5f, 1.0f, 0.0f );

		// Color for frustum is set by CSGFrustum()		

		g_LeafVis->leafIndex = leafIndex;
		switch( mat_leafvis.GetInt() )
		{
		case 2:
			{
				const mleaf_t *pLeaf = host_state.worldmodel->brush.pShared->leafs;
				int leafCount = host_state.worldmodel->brush.pShared->numleafs;
				int visCluster = pLeaf[leafIndex].cluster;
				// do entire viscluster
				for ( int i = 0; i < leafCount; i++ )
				{
					if ( pLeaf[i].cluster == visCluster )
					{
						AddLeafPortals( g_LeafVis, i );
					}
				}
			}
			break;
		case 3:
			{
				// do entire pvs
				byte pvs[ MAX_MAP_LEAFS/8 ];
				const mleaf_t *pLeaf = host_state.worldmodel->brush.pShared->leafs;
				int leafCount = host_state.worldmodel->brush.pShared->numleafs;
				int visCluster = pLeaf[leafIndex].cluster;
				CM_Vis( pvs, sizeof( pvs ), visCluster, DVIS_PVS );

				const CViewSetup &view = g_EngineRenderer->ViewGetCurrent();
				Frustum_t frustum;
				GeneratePerspectiveFrustum( g_EngineRenderer->ViewOrigin(), g_EngineRenderer->ViewAngles(), g_EngineRenderer->GetZNear(), g_EngineRenderer->GetZFar(), g_EngineRenderer->GetFov(), view.m_flAspectRatio, frustum );

				// Add frustum faces to render list in draw group DG_FRUSTUM
				CSGFrustum( frustum );

				int nPVSLeafCount = 0, nFrustumLeafCount = 0, nOutOfPVSCount = 0;
				for ( int i = 0; i < leafCount; i++ )
				{
					int cluster = pLeaf[i].cluster;
					if ( cluster >= 0 )
					{
						if ( pLeaf[i].m_vecHalfDiagonal.x < DIST_EPSILON || pLeaf[i].m_vecHalfDiagonal.y < DIST_EPSILON || pLeaf[i].m_vecHalfDiagonal.z < DIST_EPSILON )
						{
							// 0-volume leaf, ignore
							continue;
						}

						if ( (pvs[cluster>>3] & (1<<(cluster&7))) )
						{
							++ nPVSLeafCount;
							
							bool bIsVisible = !CullNodeSIMD( frustum, ( mnode_t * )( &pLeaf[i] ) );
							if ( bIsVisible ) 
							{
								++ nFrustumLeafCount;
							}
							// Add leaf portals to a different draw group based on whether they pass the frustum test (and are in PVS) or are simply in the PVS but not in the frustum
							AddLeafPortals( g_LeafVis, i, bIsVisible ? DG_FRUSTUM_VISIBLE_LEAVES : DG_PVS_VISIBLE_LEAVES );
						}
						else
						{
							++ nOutOfPVSCount;
							// Add leaf portals to a different draw group based on whether they pass the frustum test (and are in PVS) or are simply in the PVS but not in the frustum
							AddLeafPortals( g_LeafVis, i, DG_PVS_INVISIBLE_LEAVES );
						}
					}
				}
				if ( bSpewOnLeafChanged )
				{
					DevMsg( 1, "%d Leaves in PVS, %d visible, %d outside of PVS\n", nPVSLeafCount, nFrustumLeafCount, nOutOfPVSCount );
				}
			}
			break;
		case 0:
		default:
			AddLeafPortals( g_LeafVis, leafIndex );
			break;
		}
	}
}


#ifndef DEDICATED
void DrawLeafvis( leafvis_t *pVis )
{
	CMatRenderContextPtr pRenderContext( materials );

	int nMask = mat_leafvis_draw_mask.GetInt();

	for ( int nPass = 0; nPass < MAX_DRAW_GROUPS; ++ nPass )
	{
		int nVertIndex = 0;

		if ( ( ( 1 << nPass ) & nMask ) == 0 )
		{
			continue;
		}

		g_materialLeafVisWireframe->ColorModulate( pVis->m_Colors[nPass][0], pVis->m_Colors[nPass][1], pVis->m_Colors[nPass][2] );

		pRenderContext->Bind( g_materialLeafVisWireframe );
		for ( int i = 0; i < pVis->m_Polygons.Count(); i++ )
		{
			int nPolygonVertCount = pVis->m_Polygons[i].m_nVertCount;
			if ( nPolygonVertCount >= 3 && nPass == pVis->m_Polygons[i].m_DrawGroup )
			{
				IMesh *pMesh = pRenderContext->GetDynamicMesh();
				CMeshBuilder meshBuilder;
				meshBuilder.Begin( pMesh, MATERIAL_LINES, nPolygonVertCount );
				for ( int j = 0; j < nPolygonVertCount; j++ )
				{
					meshBuilder.Position3fv( pVis->verts[ nVertIndex + j ].Base() );
					meshBuilder.AdvanceVertex();
					meshBuilder.Position3fv( pVis->verts[ nVertIndex + ( ( j + 1 ) % nPolygonVertCount ) ].Base() );
					meshBuilder.AdvanceVertex();
				}
				meshBuilder.End();
				pMesh->Draw();
			}
			nVertIndex += nPolygonVertCount;
		}
	}
}

void DrawLeafvis_Solid( leafvis_t *pVis )
{
	CMatRenderContextPtr pRenderContext( materials );

	int vert = 0;
	
	Vector lightNormal(1,1,1);
	VectorNormalize(lightNormal);
	pRenderContext->Bind( g_pMaterialDebugFlat );
	for ( int i = 0; i < pVis->m_Polygons.Count(); i++ )
	{
		int vertCount = pVis->m_Polygons[i].m_nVertCount;
		if ( vertCount >= 3 )
		{
			IMesh *pMesh = pRenderContext->GetDynamicMesh( );
			CMeshBuilder meshBuilder;
			int triangleCount = vertCount-2;
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, triangleCount );
			Vector e0 = pVis->verts[vert+1] - pVis->verts[vert];
			Vector e1 = pVis->verts[vert+2] - pVis->verts[vert];
			Vector normal = CrossProduct(e1,e0);
			VectorNormalize( normal );
			float light = 0.5f + (DotProduct(normal, lightNormal)*0.5f);
			Vector color = pVis->m_Colors[DG_BASE] * light;

			for ( int j = 0; j < vertCount; j++ )
			{
				meshBuilder.Position3fv( pVis->verts[ vert + j ].Base() );
				meshBuilder.Color3fv( color.Base() );
				meshBuilder.AdvanceVertex();
			}
			for ( int j = 0; j < triangleCount; j++ )
			{
				meshBuilder.FastIndex(0);
				meshBuilder.FastIndex(j+2);
				meshBuilder.FastIndex(j+1);
			}
			meshBuilder.End();
			pMesh->Draw();
		}
		vert += vertCount;
	}
}

int FindMinBrush( CCollisionBSPData *pBSPData, int nodenum, int brushIndex )
{
	while (1)
	{
		if (nodenum < 0)
		{
			int leafIndex = -1 - nodenum;
			cleaf_t &leaf = pBSPData->map_leafs[leafIndex];
			int firstbrush = pBSPData->map_leafbrushes[ leaf.firstleafbrush ];
			if ( firstbrush < brushIndex )
			{
				brushIndex = firstbrush;
			}
			return brushIndex;
		}

		cnode_t &node = pBSPData->map_rootnode[nodenum];
		brushIndex = FindMinBrush( pBSPData, node.children[0], brushIndex );
		nodenum = node.children[1];
	}

	return brushIndex;
}

void RecomputeClipbrushes( bool bEnabled )
{
	for ( int v = 0; v < 4; v++ )
	{
		delete g_ClipVis[v];
		g_ClipVis[v] = NULL;
	}

	if ( !bEnabled )
		return;

	for ( int v = 0; v < 4; v++ )
	{
		int contents[4] = {CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP, CONTENTS_MONSTERCLIP, CONTENTS_PLAYERCLIP, CONTENTS_GRENADECLIP};
		g_ClipVis[v] = new leafvis_t;
		if ( v == 3 )
		{
			g_ClipVis[v]->m_Colors[DG_BASE].Init( 0.0f, 0.8f, 0.0f );
		}
		else
		{
			g_ClipVis[v]->m_Colors[DG_BASE].Init( v != 1 ? 1.0f : 0.5, 0.0f, v != 0 ? 1.0f : 0.0f );
		}
		CCollisionBSPData *pBSP = GetCollisionBSPData();
		int lastBrush = pBSP->numbrushes; 
		if ( pBSP->numcmodels > 1 )
		{
			lastBrush = FindMinBrush( pBSP, pBSP->map_cmodels[1].headnode, lastBrush );
		}
		for ( int i = 0; i < lastBrush; i++ )
		{
			cbrush_t *pBrush = &pBSP->map_brushes[i];
			if ( (pBrush->contents & (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP|CONTENTS_GRENADECLIP)) == contents[v] )
			{
				CUtlVector<cplane_t> planeList;
				if ( pBrush->IsBox() )
				{
					cboxbrush_t *pBox = &pBSP->map_boxbrushes[pBrush->GetBox()];
					for ( int i = 0; i < 3; i++ )
					{
						Vector normal = vec3_origin;
						normal[i] = 1.0f;
						AddPlaneToList( planeList, normal, pBox->maxs[i], true );
						AddPlaneToList( planeList, -normal, -pBox->mins[i], true );
					}
				}
				else
				{
					for ( int j = 0; j < pBrush->numsides; j++ )
					{
						cbrushside_t *pSide = &pBSP->map_brushsides[pBrush->firstbrushside + j];
						if ( pSide->bBevel )
							continue;
						AddPlaneToList( planeList, pSide->plane->normal, pSide->plane->dist, true );
					}
				}
				CSGPlaneList( g_ClipVis[v], planeList );
			}
		}
	}
}

// NOTE: UNDONE: This doesn't work on brush models - only the world.
void ClipChanged( IConVar *pConVar, const char *pOld, float flOldValue )
{
	ConVarRef clipVar( pConVar );
	RecomputeClipbrushes( clipVar.GetBool() );
}

static ConVar r_drawclipbrushes( "r_drawclipbrushes", "0", FCVAR_CHEAT, "Draw clip brushes (red=NPC+player, pink=player, purple=NPC)", ClipChanged );

static Vector LeafAmbientSamplePos( int leafIndex, const mleafambientlighting_t &sample )
{
	mleaf_t *pLeaf = &host_state.worldbrush->leafs[leafIndex];
	Vector out = pLeaf->m_vecCenter - pLeaf->m_vecHalfDiagonal;
	out.x += float(sample.x) * pLeaf->m_vecHalfDiagonal.x * (2.0f / 255.0f);
	out.y += float(sample.y) * pLeaf->m_vecHalfDiagonal.y * (2.0f / 255.0f);
	out.z += float(sample.z) * pLeaf->m_vecHalfDiagonal.z * (2.0f / 255.0f);
	
	return out;
}

// convert color formats
static void ColorRGBExp32ToColor32( const ColorRGBExp32 &color, color32 &out )
{
	Vector tmp;
	ColorRGBExp32ToVector( color, tmp );
	out.r = LinearToScreenGamma(tmp.x);
	out.g = LinearToScreenGamma(tmp.y);
	out.b = LinearToScreenGamma(tmp.z);
}

// some simple helpers to draw a cube in the special way the ambient visualization wants
static Vector CubeSide( const Vector &pos, float size, int vert )
{
	Vector side = pos;
	side.x += (vert & 1) ? -size : size;
	side.y += (vert & 2) ? -size : size;
	side.z += (vert & 4) ? -size : size;
	return side;
}

static void CubeFace( CMeshBuilder &meshBuilder, const Vector org, int v0, int v1, int v2, int v3, float size, const color32 &color )
{
	meshBuilder.Position3fv( CubeSide(org,size,v0).Base() );
	meshBuilder.Color4ubv( (byte *)&color );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3fv( CubeSide(org,size,v1).Base() );
	meshBuilder.Color4ubv( (byte *)&color );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3fv( CubeSide(org,size,v2).Base() );
	meshBuilder.Color4ubv( (byte *)&color );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3fv( CubeSide(org,size,v3).Base() );
	meshBuilder.Color4ubv( (byte *)&color );
	meshBuilder.AdvanceVertex();
}


//-----------------------------------------------------------------------------
// Purpose: Draw the leaf geometry that was computed by LeafVisBuild()
//-----------------------------------------------------------------------------
void LeafVisDraw( void )
{
	if ( g_FrustumVis )
	{
		DrawLeafvis( g_FrustumVis );
	}
	if ( g_LeafVis )
	{
		DrawLeafvis( g_LeafVis );
	}
	if ( g_ClipVis[0] )
	{
		if ( !g_ClipVis[0]->IsValid() )
		{
			RecomputeClipbrushes(true);
		}
		if ( r_drawclipbrushes.GetInt() == 2 )
		{
			DrawLeafvis_Solid( g_ClipVis[0] );
			DrawLeafvis_Solid( g_ClipVis[1] );
			DrawLeafvis_Solid( g_ClipVis[2] );
		}
		else if ( r_drawclipbrushes.GetInt() == 3 )
		{
			DrawLeafvis_Solid( g_ClipVis[3] ); // only grenade clip
		}
		else
		{
			DrawLeafvis( g_ClipVis[0] );
			DrawLeafvis( g_ClipVis[1] );
			DrawLeafvis( g_ClipVis[2] );
			DrawLeafvis( g_ClipVis[3] );
		}
	}

	if ( g_LeafVis && r_visambient.GetBool() )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( g_pMaterialDebugFlat );
		float cubesize = 12.0f;
		int leafIndex = g_LeafVis->leafIndex;
		mleafambientindex_t *pAmbient = &host_state.worldbrush->m_pLeafAmbient[leafIndex];
		if ( !pAmbient->ambientSampleCount && pAmbient->firstAmbientSample )
		{
			// this leaf references another leaf, move there (this leaf is a solid leaf so it borrows samples from a neighbor)
			leafIndex = pAmbient->firstAmbientSample;
			pAmbient = &host_state.worldbrush->m_pLeafAmbient[leafIndex];
		}
		for ( int i = 0; i < pAmbient->ambientSampleCount; i++ )
		{
			IMesh *pMesh = pRenderContext->GetDynamicMesh( );
			CMeshBuilder meshBuilder;
			meshBuilder.Begin( pMesh, MATERIAL_QUADS, 6 );
			const mleafambientlighting_t &sample = host_state.worldbrush->m_pAmbientSamples[pAmbient->firstAmbientSample+i];
			Vector pos = LeafAmbientSamplePos( leafIndex, sample );
			// x axis
			color32 color;
			ColorRGBExp32ToColor32( sample.cube.m_Color[0], color );	// x
			CubeFace( meshBuilder, pos, 4, 6, 2, 0, cubesize, color );
			ColorRGBExp32ToColor32( sample.cube.m_Color[1], color );	// -x
			CubeFace( meshBuilder, pos, 7, 5, 1, 3, cubesize, color );
			ColorRGBExp32ToColor32( sample.cube.m_Color[2], color );	// y
			CubeFace( meshBuilder, pos, 0, 1, 5, 4, cubesize, color );
			ColorRGBExp32ToColor32( sample.cube.m_Color[3], color );	// -y
			CubeFace( meshBuilder, pos, 3, 2, 6, 7, cubesize, color );
			ColorRGBExp32ToColor32( sample.cube.m_Color[4], color );	// z
			CubeFace( meshBuilder, pos, 2, 3, 1, 0, cubesize, color );
			ColorRGBExp32ToColor32( sample.cube.m_Color[5], color );	// -z
			CubeFace( meshBuilder, pos, 4, 5, 7, 6, cubesize, color );
			meshBuilder.End();
			pMesh->Draw();
		}
	}
}


void CSGFrustum( Frustum_t &frustum )
{
	delete g_FrustumVis;
	g_FrustumVis = new leafvis_t;

	g_FrustumVis->m_Colors[DG_FRUSTUM].Init(1.0f, 1.0f, 1.0f);
	CUtlVector<cplane_t> planeList;
	for ( int i = 0; i < 6; i++ )
	{
		cplane_t tmp;
		tmp.type = PLANE_ANYZ;
		frustum.GetPlane(i, &tmp.normal, &tmp.dist);
		planeList.AddToTail( tmp );
	}
	CSGPlaneList( g_FrustumVis, planeList, DG_FRUSTUM );
}
#endif
