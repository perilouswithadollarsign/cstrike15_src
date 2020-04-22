//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: BSP Building tool
//
// $NoKeywords: $
//=============================================================================//

#include "vbsp.h"
#include "detail.h"
#include "physdll.h"
#include "utilmatlib.h"
#include "disp_vbsp.h"
#include "writebsp.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "map.h"
#include "tools_minidump.h"
#include "materialsub.h"
#include "loadcmdline.h"
#include "byteswap.h"
#include "worldvertextransitionfixup.h"
#include "lzma/lzma.h"
#include "tier1/UtlBuffer.h"

extern float		g_maxLightmapDimension;

char		source[1024];
char		mapbase[ 64 ];
char		name[1024];
char		materialPath[1024];

vec_t		microvolume = 1.0;
qboolean	noprune;
qboolean	glview;
qboolean	nodetail;
qboolean	fulldetail;
qboolean	onlyents;
bool		onlyprops;
qboolean	nomerge;
qboolean	nomergewater = false;
qboolean	nowater;
qboolean	nocsg;
qboolean	noweld;
qboolean	noshare;
qboolean	nosubdiv;
qboolean	notjunc;
qboolean	noopt;
qboolean	leaktest;
qboolean	verboseentities;
qboolean	staticpropcombine = false;
qboolean	staticpropcombine_delsources = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_IGNORE_NORMALS = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_NO_SHADOW = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_NO_FLASHLIGHT = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_MARKED_FOR_FAST_REFLECTION = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_NO_PER_VERTEX_LIGHTING = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_NO_SELF_SHADOWING = true;
qboolean	staticpropcombine_doflagcompare_STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH = true;
qboolean	staticpropcombine_considervis = false;
qboolean	staticpropcombine_autocombine = false;
qboolean	staticpropcombine_suggestcombinerules = false;
int			g_nAutoCombineMinInstances = 2;
qboolean	dumpcollide = false;
qboolean	g_bLowPriority = false;
qboolean	g_DumpStaticProps = false;
qboolean	g_bSkyVis = false;			// skybox vis is off by default, toggle this to enable it
bool		g_bLightIfMissing = false;
bool		g_snapAxialPlanes = false;
bool		g_bKeepStaleZip = false;
bool		g_NodrawTriggers = false;
bool		g_DisableWaterLighting = false;
bool		g_bAllowDetailCracks = false;
bool		g_bNoVirtualMesh = false;
int			g_nVisGranularityX = 0, g_nVisGranularityY = 0, g_nVisGranularityZ = 0;

float		g_defaultLuxelSize = DEFAULT_LUXEL_SIZE;
float		g_luxelScale = 1.0f;
float		g_minLuxelScale = 1.0f;
float		g_maxLuxelScale = 999999.0f;
bool		g_BumpAll = false;

// Convert structural BSP brushes (which affect visibility) to detail brushes
bool		g_bConvertStructureToDetail = false;

CUtlVector<int> g_SkyAreas;
char		outbase[32];

// HLTOOLS: Introduce these calcs to make the block algorithm proportional to the proper 
// world coordinate extents.  Assumes square spatial constraints.

int g_nBlockSize = 1024;

#define BLOCKS_SPACE	(COORD_EXTENT/g_nBlockSize)
#define BLOCKX_OFFSET	((BLOCKS_SPACE/2)+1)
#define BLOCKY_OFFSET	((BLOCKS_SPACE/2)+1)
#define BLOCKS_MIN		(-(BLOCKS_SPACE/2))
#define BLOCKS_MAX		((BLOCKS_SPACE/2)-1)
#define BLOCKS_ARRAY_WIDTH	( BLOCKS_SPACE + 2 )

int			block_xl = BLOCKS_MIN, block_xh = BLOCKS_MAX, block_yl = BLOCKS_MIN, block_yh = BLOCKS_MAX;

int			entity_num;


node_t		**ppBlockNodes = NULL;

//-----------------------------------------------------------------------------
// Assign occluder areas (must happen *after* the world model is processed)
//-----------------------------------------------------------------------------
void AssignOccluderAreas( tree_t *pTree );
static void Compute3DSkyboxAreas( node_t *headnode, CUtlVector<int>& areas );


/*
============
BlockTree

============
*/
node_t	*BlockTree ( int xl, int yl, int xh, int yh )
{
	node_t	*pNode;
	Vector	normal;
	float	dist;
	int		mid;
	
	if ( xl == xh && yl == yh )
	{
		pNode = ppBlockNodes[ xl+BLOCKX_OFFSET + ( ( yl+BLOCKY_OFFSET ) * BLOCKS_ARRAY_WIDTH ) ];
		if ( !pNode )
		{	// return an empty leaf
			pNode = AllocNode ();
			pNode->planenum = PLANENUM_LEAF;
			pNode->contents = 0; //CONTENTS_SOLID;
			return pNode;
		}
		return pNode;
	}

	// create a seperator along the largest axis
	pNode = AllocNode ();

	if ( xh - xl > yh - yl )
	{	// split x axis
		mid = xl + (xh-xl)/2 + 1;
		normal[0] = 1;
		normal[1] = 0;
		normal[2] = 0;
		dist = mid*g_nBlockSize;
		pNode->planenum = g_MainMap->FindFloatPlane (normal, dist);
		pNode->children[0] = BlockTree ( mid, yl, xh, yh);
		pNode->children[1] = BlockTree ( xl, yl, mid-1, yh);
	}
	else
	{
		mid = yl + (yh-yl)/2 + 1;
		normal[0] = 0;
		normal[1] = 1;
		normal[2] = 0;
		dist = mid*g_nBlockSize;
		pNode->planenum = g_MainMap->FindFloatPlane (normal, dist);
		pNode->children[0] = BlockTree ( xl, mid, xh, yh);
		pNode->children[1] = BlockTree ( xl, yl, xh, mid-1);
	}

	return pNode;
}

/*
============
ProcessBlock_Thread

============
*/
int			brush_start, brush_end;
void ProcessBlock_Thread (int threadnum, int blocknum)
{
	int		xblock, yblock;
	Vector		mins, maxs;
	bspbrush_t	*brushes;
	tree_t		*tree;
	node_t		*node;

	yblock = block_yl + blocknum / (block_xh-block_xl+1);
	xblock = block_xl + blocknum % (block_xh-block_xl+1);

	qprintf ("############### block %2i,%2i ###############\n", xblock, yblock);

	mins[0] = xblock*g_nBlockSize;
	mins[1] = yblock*g_nBlockSize;
	mins[2] = MIN_COORD_INTEGER;
	maxs[0] = (xblock+1)*g_nBlockSize;
	maxs[1] = (yblock+1)*g_nBlockSize;
	maxs[2] = MAX_COORD_INTEGER;

	// the makelist and chopbrushes could be cached between the passes...
	brushes = MakeBspBrushList (brush_start, brush_end, mins, maxs, NO_DETAIL);
	if (!brushes)
	{
		node = AllocNode ();
		node->planenum = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		ppBlockNodes[ xblock+BLOCKX_OFFSET + ( ( yblock+BLOCKY_OFFSET ) * BLOCKS_ARRAY_WIDTH ) ] = node;
		return;
	}    

	FixupAreaportalWaterBrushes( brushes );
	if (!nocsg)
		brushes = ChopBrushes (brushes);

	tree = BrushBSP (brushes, mins, maxs);
	
	ppBlockNodes[ xblock+BLOCKX_OFFSET + ( ( yblock+BLOCKY_OFFSET ) * BLOCKS_ARRAY_WIDTH ) ] = tree->headnode;
}


/*
============
ProcessWorldModel

============
*/
void SplitSubdividedFaces( node_t *headnode ); // garymcthack
void ProcessWorldModel (void)
{
	entity_t	*e;
	tree_t		*tree = NULL;
	qboolean	leaked;
	int	optimize;
	int			start;

	e = &entities[entity_num];

	brush_start = e->firstbrush;
	brush_end = brush_start + e->numbrushes;
	leaked = false;

	if ( ppBlockNodes == NULL )
	{
		ppBlockNodes = new node_t *[ BLOCKS_ARRAY_WIDTH * BLOCKS_ARRAY_WIDTH ]; 
		Q_memset( ppBlockNodes, 0, BLOCKS_ARRAY_WIDTH * BLOCKS_ARRAY_WIDTH * sizeof( node_t* ) );
	}

	//
	// perform per-block operations
	//
	if (block_xh * g_nBlockSize > g_MainMap->map_maxs[0])
	{
		block_xh = floor(g_MainMap->map_maxs[0]/g_nBlockSize);
	}
	if ( (block_xl+1) * g_nBlockSize < g_MainMap->map_mins[0])
	{
		block_xl = floor(g_MainMap->map_mins[0]/g_nBlockSize);
	}
	if (block_yh * g_nBlockSize > g_MainMap->map_maxs[1])
	{
		block_yh = floor(g_MainMap->map_maxs[1]/g_nBlockSize);
	}
	if ( (block_yl+1) * g_nBlockSize < g_MainMap->map_mins[1])
	{
		block_yl = floor(g_MainMap->map_mins[1]/g_nBlockSize);
	}

	// HLTOOLS: updated to +/- MAX_COORD_INTEGER ( new world size limits / worldsize.h )
	if (block_xl < BLOCKS_MIN)
	{
		block_xl = BLOCKS_MIN;
	}
	if (block_yl < BLOCKS_MIN)
	{
		block_yl = BLOCKS_MIN;
	}
	if (block_xh > BLOCKS_MAX)
	{
		block_xh = BLOCKS_MAX;
	}
	if (block_yh > BLOCKS_MAX)
	{
		block_yh = BLOCKS_MAX;
	}

	for (optimize = 0 ; optimize <= 1 ; optimize++)
	{
		qprintf ("--------------------------------------------\n");

		RunThreadsOnIndividual ((block_xh-block_xl+1)*(block_yh-block_yl+1),
			!verbose, ProcessBlock_Thread);

		//
		// build the division tree
		// oversizing the blocks guarantees that all the boundaries
		// will also get nodes.
		//

		qprintf ("--------------------------------------------\n");

		tree = AllocTree ();
		tree->headnode = BlockTree (block_xl-1, block_yl-1, block_xh+1, block_yh+1);

		tree->mins[0] = (block_xl)*g_nBlockSize;
		tree->mins[1] = (block_yl)*g_nBlockSize;
		tree->mins[2] = g_MainMap->map_mins[2] - 8;

		tree->maxs[0] = (block_xh+1)*g_nBlockSize;
		tree->maxs[1] = (block_yh+1)*g_nBlockSize;
		tree->maxs[2] = g_MainMap->map_maxs[2] + 8;

		//
		// perform the global operations
		//

		// make the portals/faces by traversing down to each empty leaf
		MakeTreePortals (tree);

		if (FloodEntities (tree))
		{
			// turns everthing outside into solid
			FillOutside (tree->headnode);
		}
		else
		{
			Warning( ("**** leaked ****\n") );
			leaked = true;
			LeakFile (tree);
			if (leaktest)
			{
				Warning( ("--- MAP LEAKED ---\n") );
				exit (0);
			}
		}

		// mark the brush sides that actually turned into faces
		MarkVisibleSides (tree, brush_start, brush_end, NO_DETAIL);
		if (noopt || leaked)
			break;
		if (!optimize)
		{
			// If we are optimizing, free the tree.  Next time we will construct it again, but
			// we'll use the information in MarkVisibleSides() so we'll only split with planes that
			// actually contribute renderable geometry
			FreeTree (tree);
		}
	}

	FloodAreas (tree);

	RemoveAreaPortalBrushes_R( tree->headnode );

	start = Plat_FloatTime();
	Msg("Building Faces...");
	// this turns portals with one solid side into faces
	// it also subdivides each face if necessary to fit max lightmap dimensions
	MakeFaces (tree->headnode);
	Msg("done (%d)\n", (int)(Plat_FloatTime() - start) );

	if (glview)
	{
		WriteGLView (tree, source);
	}

	AssignOccluderAreas( tree );
	Compute3DSkyboxAreas( tree->headnode, g_SkyAreas );
	face_t *pLeafFaceList = NULL;
	if ( !nodetail )
	{
		pLeafFaceList = MergeDetailTree( tree, brush_start, brush_end );
	}

	start = Plat_FloatTime();

	Msg("FixTjuncs...\n");
	
	// This unifies the vertex list for all edges (splits collinear edges to remove t-junctions)
	// It also welds the list of vertices out of each winding/portal and rounds nearly integer verts to integer
	pLeafFaceList = FixTjuncs (tree->headnode, pLeafFaceList);

	// this merges all of the solid nodes that have separating planes
	if (!noprune)
	{
		Msg("PruneNodes...\n");
		PruneNodes (tree->headnode);
	}

//	Msg( "SplitSubdividedFaces...\n" );
//	SplitSubdividedFaces( tree->headnode );

	Msg("WriteBSP...\n");
	WriteBSP (tree->headnode, pLeafFaceList);
	Msg("done (%d)\n", (int)(Plat_FloatTime() - start) );

	if (!leaked)
	{
		WritePortalFile (tree);
	}

	FreeTree( tree );
	FreeLeafFaces( pLeafFaceList );
}

/*
============
ProcessSubModel

============
*/
void ProcessSubModel( )
{
	entity_t	*e;
	int			start, end;
	tree_t		*tree;
	bspbrush_t	*list;
	Vector		mins, maxs;

	e = &entities[entity_num];

	start = e->firstbrush;
	end = start + e->numbrushes;

	mins[0] = mins[1] = mins[2] = MIN_COORD_INTEGER;
	maxs[0] = maxs[1] = maxs[2] = MAX_COORD_INTEGER;
	list = MakeBspBrushList (start, end, mins, maxs, FULL_DETAIL);

	if (!nocsg)
		list = ChopBrushes (list);
	tree = BrushBSP (list, mins, maxs);
	
	// This would wind up crashing the engine because we'd have a negative leaf index in dmodel_t::headnode.
	if ( tree->headnode->planenum == PLANENUM_LEAF )
	{
		const char *pClassName = ValueForKey( e, "classname" );
		const char *pTargetName = ValueForKey( e, "targetname" );
		Error( "bmodel %d has no head node (class '%s', targetname '%s')", entity_num, pClassName, pTargetName );
	}

	MakeTreePortals (tree);
	
#if DEBUG_BRUSHMODEL
	if ( entity_num == DEBUG_BRUSHMODEL )
		WriteGLView( tree, "tree_all" );
#endif

	MarkVisibleSides (tree, start, end, FULL_DETAIL);
	MakeFaces (tree->headnode);

	FixTjuncs( tree->headnode, NULL );
	WriteBSP( tree->headnode, NULL );
	
#if DEBUG_BRUSHMODEL
	if ( entity_num == DEBUG_BRUSHMODEL )
	{
		WriteGLView( tree, "tree_vis" );
		WriteGLViewFaces( tree, "tree_faces" );
	}
#endif

	FreeTree (tree);
}

//-----------------------------------------------------------------------------
// Helper routine to setup side and texture setting of a splitting hint brush
//-----------------------------------------------------------------------------
bool InsertVisibilitySplittingHintBrush( Vector const &cut0, Vector const &cut1, Vector const &cut2, Vector const &cut3, Vector vNormal, int &nSideID, int &nBrushID )
{
	if ( g_MainMap->nummapbrushes == MAX_MAP_BRUSHES )
	{
		Error( "nummapbrushes == MAX_MAP_BRUSHES when inserting visibility split hint brushes" );
	}

	mapbrush_t &mbr = g_MainMap->mapbrushes[g_MainMap->nummapbrushes];
	V_memset( &mbr, 0, sizeof( mbr ) );
	mbr.brushnum = g_MainMap->nummapbrushes;
	mbr.id = ( ++ nBrushID );
	mbr.numsides = 6;
	mbr.original_sides = &g_MainMap->brushsides[g_MainMap->nummapbrushsides];
	g_MainMap->nummapbrushes ++;

	//
	// HINT
	//
	{
		if ( g_MainMap->nummapbrushsides == MAX_MAP_BRUSHSIDES )
		{
			Error( "nummapbrushsides == MAX_MAP_BRUSHSIDES when inserting visibility split hint brushes" );
		}
		side_t &side = g_MainMap->brushsides[g_MainMap->nummapbrushsides];
		V_memset( &side, 0, sizeof( side ) );
		side.planenum = g_MainMap->PlaneFromPoints( cut0, cut1, cut2 );
		side.id = ( ++ nSideID );
		side.visible = true;
		side.thin = true;
		side.surf = ( SURF_NODRAW | SURF_NOLIGHT | SURF_HINT );
		side.texinfo = FindMiptex( "TOOLS/TOOLSHINT" );

		brush_texture_t &btt = g_MainMap->side_brushtextures[g_MainMap->nummapbrushsides];
		V_memset( &btt, 0, sizeof( btt ) );
		V_strcpy_safe( btt.name, "TOOLS/TOOLSHINT" );
		btt.flags = ( SURF_NODRAW | SURF_NOLIGHT | SURF_HINT );
		btt.lightmapWorldUnitsPerLuxel = 16;
		btt.textureWorldUnitsPerTexel[0] = btt.textureWorldUnitsPerTexel[1] = 0.25f;
		btt.UAxis[0] = 1;
		btt.VAxis[2] = -1;

		g_MainMap->nummapbrushsides ++;
	}

	// SKIP points array
	Vector arrSkipPoints[15] =
	{
		cut2 + vNormal, cut1 + vNormal, cut0 + vNormal, // LARGE skip
		cut1 + vNormal, cut1, cut0, // small skip 0->1->1'
		cut2 + vNormal, cut2, cut1, // small skip 1->2->2'
		cut3 + vNormal, cut3, cut2, // small skip 3'->3->2
		cut0 + vNormal, cut0, cut3, // small skip 3->0->0'
	};

	for ( int iSkip = 0; iSkip < 5; ++ iSkip )
	{
		if ( g_MainMap->nummapbrushsides == MAX_MAP_BRUSHSIDES )
		{
			Error( "nummapbrushsides == MAX_MAP_BRUSHSIDES when inserting visibility split hint brushes" );
		}

		side_t &side = g_MainMap->brushsides[g_MainMap->nummapbrushsides];
		V_memset( &side, 0, sizeof( side ) );
		side.planenum = g_MainMap->PlaneFromPoints( arrSkipPoints[ iSkip*3 + 0 ], arrSkipPoints[ iSkip*3 + 1 ], arrSkipPoints[ iSkip*3 + 2 ] );
		side.id = ( ++ nSideID );
		side.visible = false;
		side.thin = true;
		side.surf = ( SURF_NODRAW | SURF_NOLIGHT | SURF_SKIP );
		side.texinfo = FindMiptex( "TOOLS/TOOLSSKIP" );

		brush_texture_t &btt = g_MainMap->side_brushtextures[g_MainMap->nummapbrushsides];
		V_memset( &btt, 0, sizeof( btt ) );
		V_strcpy_safe( btt.name, "TOOLS/TOOLSSKIP" );
		btt.flags = ( SURF_NODRAW | SURF_NOLIGHT | SURF_SKIP );
		btt.lightmapWorldUnitsPerLuxel = 16;
		btt.textureWorldUnitsPerTexel[0] = btt.textureWorldUnitsPerTexel[1] = 0.25f;
		btt.UAxis[0] = 1;
		btt.VAxis[2] = -1;

		g_MainMap->nummapbrushsides ++;
	}
	
	g_MainMap->MakeBrushWindings( &mbr );

	return true;
}

//-----------------------------------------------------------------------------
// Inserts visibility splitting hint brushes
//-----------------------------------------------------------------------------
void InsertVisibilitySplittingHintBrushes()
{
	// Compute max brush side ID
	int		max_side_id = 0;
	for( int i = 0; i < g_MainMap->nummapbrushsides; i++ )
	{
		if ( g_MainMap->brushsides[ i ].id > max_side_id )
		{
			max_side_id = g_MainMap->brushsides[ i ].id;
		}
	}

	// Compute max brush ID
	int		max_brush_id = 0;
	for( int i = 0; i < g_MainMap->nummapbrushes; i++ )
	{
		if ( g_MainMap->mapbrushes[ i ].id > max_brush_id )
		{
			max_brush_id = g_MainMap->mapbrushes[ i ].id;
		}
	}

	// Have a fake entity tracking all the splits that we added
	entity_t entityForVisibilitySplits;
	V_memset( &entityForVisibilitySplits, 0, sizeof( entityForVisibilitySplits ) );
	entityForVisibilitySplits.firstbrush = g_MainMap->nummapbrushes;

	// Force visibility splits
	if ( g_nVisGranularityX > 0 )
	{
		int nMinX = g_MainMap->map_mins.x;
		nMinX = 1 + ( nMinX / g_nVisGranularityX ) * g_nVisGranularityX;
		int nMaxX = g_MainMap->map_maxs.x;
		nMaxX = -1 + ( nMaxX / g_nVisGranularityX ) * g_nVisGranularityX;
		int numCuts = 0;
		for ( ; nMinX < nMaxX; nMinX += g_nVisGranularityX )
		{
			Vector cut0( nMinX, g_MainMap->map_maxs.y, g_MainMap->map_maxs.z );
			Vector cut1( nMinX, g_MainMap->map_mins.y, g_MainMap->map_maxs.z );
			Vector cut2( nMinX, g_MainMap->map_mins.y, g_MainMap->map_mins.z );
			Vector cut3( nMinX, g_MainMap->map_maxs.y, g_MainMap->map_mins.z );
			Vector vNormal( 1, 0, 0 );
			if ( InsertVisibilitySplittingHintBrush( cut0, cut1, cut2, cut3, vNormal, max_side_id, max_brush_id ) )
			{
				++ entityForVisibilitySplits.numbrushes;
				++ numCuts;
			}
		}
		Msg("Vis granularity X introduced %i cuts between %.0f and %.0f\n", numCuts, g_MainMap->map_mins.x, g_MainMap->map_maxs.x );
	}
	if ( g_nVisGranularityY > 0 )
	{
		int nMinY = g_MainMap->map_mins.y;
		nMinY = 1 + ( nMinY / g_nVisGranularityY ) * g_nVisGranularityY;
		int nMaxY = g_MainMap->map_maxs.y;
		nMaxY = -1 + ( nMaxY / g_nVisGranularityY ) * g_nVisGranularityY;
		int numCuts = 0;
		for ( ; nMinY < nMaxY; nMinY += g_nVisGranularityY )
		{
			Vector cut0( g_MainMap->map_maxs.x, nMinY, g_MainMap->map_mins.z );
			Vector cut1( g_MainMap->map_mins.x, nMinY, g_MainMap->map_mins.z );
			Vector cut2( g_MainMap->map_mins.x, nMinY, g_MainMap->map_maxs.z );
			Vector cut3( g_MainMap->map_maxs.x, nMinY, g_MainMap->map_maxs.z );
			Vector vNormal( 0, 1, 0 );
			if ( InsertVisibilitySplittingHintBrush( cut0, cut1, cut2, cut3, vNormal, max_side_id, max_brush_id ) )
			{
				++ entityForVisibilitySplits.numbrushes;
				++ numCuts;
			}
		}
		Msg("Vis granularity Y introduced %i cuts between %.0f and %.0f\n", numCuts, g_MainMap->map_mins.y, g_MainMap->map_maxs.y );
	}
	if ( g_nVisGranularityZ > 0 )
	{
		int nMinZ = g_MainMap->map_mins.z;
		nMinZ = 1 + ( nMinZ / g_nVisGranularityZ ) * g_nVisGranularityZ;
		int nMaxZ = g_MainMap->map_maxs.z;
		nMaxZ = -1 + ( nMaxZ / g_nVisGranularityZ ) * g_nVisGranularityZ;
		int numCuts = 0;
		for ( ; nMinZ < nMaxZ; nMinZ += g_nVisGranularityZ )
		{
			Vector cut0( g_MainMap->map_mins.x, g_MainMap->map_maxs.y, nMinZ );
			Vector cut1( g_MainMap->map_mins.x, g_MainMap->map_mins.y, nMinZ );
			Vector cut2( g_MainMap->map_maxs.x, g_MainMap->map_mins.y, nMinZ );
			Vector cut3( g_MainMap->map_maxs.x, g_MainMap->map_maxs.y, nMinZ );
			Vector vNormal( 0, 0, 1 );
			if ( InsertVisibilitySplittingHintBrush( cut0, cut1, cut2, cut3, vNormal, max_side_id, max_brush_id ) )
			{
				++ entityForVisibilitySplits.numbrushes;
				++ numCuts;
			}
		}
		Msg("Vis granularity Z introduced %i cuts between %.0f and %.0f\n", numCuts, g_MainMap->map_mins.z, g_MainMap->map_maxs.z );
	}

	// Now move all the newly introduced brushes to world
	if ( entityForVisibilitySplits.numbrushes )
	{
		g_MainMap->MoveBrushesToWorld( &entityForVisibilitySplits );
		if ( num_entities != g_MainMap->num_entities )
		{
			Error( "Entities accounting error while enforcing visibility granularity!\n" );
		}
		else
		{	// Force a re-copy since moving brushes to world affected all brushes and sides
			memcpy( entities, g_MainMap->entities, sizeof( g_MainMap->entities ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Returns true if the entity is a func_occluder
//-----------------------------------------------------------------------------
bool IsFuncOccluder( int entity_num )
{
	entity_t *mapent = &entities[entity_num];
	const char *pClassName = ValueForKey( mapent, "classname" );
	return (strcmp("func_occluder", pClassName) == 0);
}


//-----------------------------------------------------------------------------
// Computes the area of a brush's occluders
//-----------------------------------------------------------------------------
float ComputeOccluderBrushArea( mapbrush_t *pBrush )
{
	float flArea = 0.0f;
	for ( int j = 0; j < pBrush->numsides; ++j )
	{
		side_t *pSide = &(pBrush->original_sides[j]);

		// Skip nodraw surfaces
		if ( texinfo[pSide->texinfo].flags & SURF_NODRAW )
			continue;

		if ( !pSide->winding )
			continue;

		flArea += WindingArea( pSide->winding );
	}

	return flArea;
}


//-----------------------------------------------------------------------------
// Clips all occluder brushes against each other
//-----------------------------------------------------------------------------
static tree_t *ClipOccluderBrushes( )
{
	// Create a list of all occluder brushes in the level
	CUtlVector< mapbrush_t * > mapBrushes( 1024, 1024 );
	for ( entity_num=0; entity_num < g_MainMap->num_entities; ++entity_num )
	{
		if (!IsFuncOccluder(entity_num))
			continue;

		entity_t *e = &entities[entity_num];
		int end = e->firstbrush + e->numbrushes;
		int i;
		for ( i = e->firstbrush; i < end; ++i )
		{
			mapBrushes.AddToTail( &g_MainMap->mapbrushes[i] );
		}
	}

	int nBrushCount = mapBrushes.Count();
	if ( nBrushCount == 0 )
		return NULL;

	Vector mins, maxs;
	mins[0] = mins[1] = mins[2] = MIN_COORD_INTEGER;
	maxs[0] = maxs[1] = maxs[2] = MAX_COORD_INTEGER;

	bspbrush_t *list = MakeBspBrushList( mapBrushes.Base(), nBrushCount, mins, maxs );

	if (!nocsg)
		list = ChopBrushes (list);
	tree_t *tree = BrushBSP (list, mins, maxs);
	MakeTreePortals (tree);
	MarkVisibleSides (tree, mapBrushes.Base(), nBrushCount);
	MakeFaces( tree->headnode );

	// NOTE: This will output the occluder face vertices + planes
	FixTjuncs( tree->headnode, NULL );

	return tree;
}


//-----------------------------------------------------------------------------
// Generate a list of unique sides in the occluder tree
//-----------------------------------------------------------------------------
static void GenerateOccluderSideList( int nEntity, CUtlVector<side_t*> &occluderSides )
{
	entity_t *e = &entities[nEntity];
	int end = e->firstbrush + e->numbrushes;
	int i, j;
	for ( i = e->firstbrush; i < end; ++i )
	{
		mapbrush_t *mb = &g_MainMap->mapbrushes[i];
		for ( j = 0; j < mb->numsides; ++j )
		{
			occluderSides.AddToTail( &(mb->original_sides[j]) );
		}
	}
}


//-----------------------------------------------------------------------------
// Generate a list of unique faces in the occluder tree
//-----------------------------------------------------------------------------
static void GenerateOccluderFaceList( node_t *pOccluderNode, CUtlVector<face_t*> &occluderFaces )
{
	if (pOccluderNode->planenum == PLANENUM_LEAF)
		return;

	for ( face_t *f=pOccluderNode->faces ; f ; f = f->next )
	{
		occluderFaces.AddToTail( f );
	}

	GenerateOccluderFaceList( pOccluderNode->children[0], occluderFaces );
	GenerateOccluderFaceList( pOccluderNode->children[1], occluderFaces );
}


//-----------------------------------------------------------------------------
// For occluder area assignment
//-----------------------------------------------------------------------------
struct OccluderInfo_t
{
	int m_nOccluderEntityIndex;
};

static CUtlVector< OccluderInfo_t > g_OccluderInfo;


//-----------------------------------------------------------------------------
// Emits occluder brushes
//-----------------------------------------------------------------------------
static void EmitOccluderBrushes()
{
	char str[64];

	g_OccluderData.RemoveAll();
	g_OccluderPolyData.RemoveAll();
	g_OccluderVertexIndices.RemoveAll();

	tree_t *pOccluderTree = ClipOccluderBrushes();
	if (!pOccluderTree)
		return;

	CUtlVector<face_t*> faceList( 1024, 1024 );
	CUtlVector<side_t*> sideList( 1024, 1024 );
	GenerateOccluderFaceList( pOccluderTree->headnode, faceList );

#ifdef _DEBUG
	int *pEmitted = (int*)stackalloc( faceList.Count() * sizeof(int) );
	memset( pEmitted, 0, faceList.Count() * sizeof(int) );
#endif

	for ( entity_num=1; entity_num < num_entities; ++entity_num )
	{
		if (!IsFuncOccluder(entity_num))
			continue;

		// Output only those parts of the occluder tree which are a part of the brush
		int nOccluder = g_OccluderData.AddToTail();
		doccluderdata_t &occluderData = g_OccluderData[ nOccluder ];
		occluderData.firstpoly = g_OccluderPolyData.Count();
		occluderData.mins.Init( FLT_MAX, FLT_MAX, FLT_MAX );
		occluderData.maxs.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		occluderData.flags = 0;
		occluderData.area = -1;

		// NOTE: If you change the algorithm by which occluder numbers are allocated,
		// then you must also change FixupOnlyEntsOccluderEntities() below
		sprintf (str, "%i", nOccluder);
		SetKeyValue (&entities[entity_num], "occludernumber", str);

		int nIndex = g_OccluderInfo.AddToTail();
		g_OccluderInfo[nIndex].m_nOccluderEntityIndex = entity_num;
		
		sideList.RemoveAll();
		GenerateOccluderSideList( entity_num, sideList );
		for ( int i = faceList.Count(); --i >= 0; )
		{
			// Skip nodraw surfaces, but not triggers that have been marked as nodraw
			face_t *f = faceList[i];
			if ( ( texinfo[f->texinfo].flags & SURF_NODRAW ) &&
				 (( texinfo[f->texinfo].flags & SURF_TRIGGER ) == 0 ) )
				continue;

			// Only emit faces that appear in the side list of the occluder
			for ( int j = sideList.Count(); --j >= 0; )
			{
				if ( sideList[j] != f->originalface )
					continue;

				if ( f->numpoints < 3 )
					continue;

				// not a final face
				Assert ( !f->merged && !f->split[0] && !f->split[1] );

#ifdef _DEBUG
				Assert( !pEmitted[i] );
				pEmitted[i] = entity_num;
#endif

				int k = g_OccluderPolyData.AddToTail();
				doccluderpolydata_t *pOccluderPoly = &g_OccluderPolyData[k];

				pOccluderPoly->planenum = f->planenum;
				pOccluderPoly->vertexcount = f->numpoints;
				pOccluderPoly->firstvertexindex = g_OccluderVertexIndices.Count();
				for( k = 0; k < f->numpoints; ++k )
				{
					g_OccluderVertexIndices.AddToTail( f->vertexnums[k] );

					const Vector &p = dvertexes[f->vertexnums[k]].point; 
					VectorMin( occluderData.mins, p, occluderData.mins );
					VectorMax( occluderData.maxs, p, occluderData.maxs );
				}

				break;
			}
		}

		occluderData.polycount = g_OccluderPolyData.Count() - occluderData.firstpoly;

		// Mark this brush as not having brush geometry so it won't be re-emitted with a brush model
		entities[entity_num].numbrushes = 0;
	}

	FreeTree( pOccluderTree );
}


//-----------------------------------------------------------------------------
// Set occluder area
//-----------------------------------------------------------------------------
void SetOccluderArea( int nOccluder, int nArea, int nEntityNum )
{
	if ( g_OccluderData[nOccluder].area <= 0 )
	{
		g_OccluderData[nOccluder].area = nArea;
	}
	else if ( (nArea != 0) && (g_OccluderData[nOccluder].area != nArea) )
	{
		const char *pTargetName = ValueForKey( &entities[nEntityNum], "targetname" );
		if (!pTargetName)
		{
			pTargetName = "<no name>";
		}
		Warning("Occluder \"%s\" straddles multiple areas. This is invalid!\n", pTargetName );
	}
}


//-----------------------------------------------------------------------------
// Assign occluder areas (must happen *after* the world model is processed)
//-----------------------------------------------------------------------------
void AssignAreaToOccluder( int nOccluder, tree_t *pTree, bool bCrossAreaPortals )
{
	int nFirstPoly = g_OccluderData[nOccluder].firstpoly;
	int nEntityNum = g_OccluderInfo[nOccluder].m_nOccluderEntityIndex;
	for ( int j = 0; j < g_OccluderData[nOccluder].polycount; ++j )
	{
		doccluderpolydata_t *pOccluderPoly = &g_OccluderPolyData[nFirstPoly + j];
		int nFirstVertex = pOccluderPoly->firstvertexindex;
		for ( int k = 0; k < pOccluderPoly->vertexcount; ++k )
		{
			int nVertexIndex = g_OccluderVertexIndices[nFirstVertex + k];
			node_t *pNode = NodeForPoint( pTree->headnode, dvertexes[ nVertexIndex ].point );

			SetOccluderArea( nOccluder, pNode->area, nEntityNum );

			int nOtherSideIndex;
			portal_t *pPortal;
			for ( pPortal = pNode->portals; pPortal; pPortal = pPortal->next[!nOtherSideIndex] )
			{
				nOtherSideIndex = (pPortal->nodes[0] == pNode) ? 1 : 0;
				if (!pPortal->onnode)
					continue;		// edge of world

				// Don't cross over area portals for the area check
				if ((!bCrossAreaPortals) && pPortal->nodes[nOtherSideIndex]->contents & CONTENTS_AREAPORTAL)
					continue;

				int nAdjacentArea = pPortal->nodes[nOtherSideIndex] ? pPortal->nodes[nOtherSideIndex]->area : 0; 
				SetOccluderArea( nOccluder, nAdjacentArea, nEntityNum );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Assign occluder areas (must happen *after* the world model is processed)
//-----------------------------------------------------------------------------
void AssignOccluderAreas( tree_t *pTree )
{
	for ( int i = 0; i < g_OccluderData.Count(); ++i )
	{
		AssignAreaToOccluder( i, pTree, false );

		// This can only have happened if the only valid portal out leads into an areaportal
		if ( g_OccluderData[i].area <= 0 )
		{
			AssignAreaToOccluder( i, pTree, true );
		}
	}
}



//-----------------------------------------------------------------------------
// Make sure the func_occluders have the appropriate data set
//-----------------------------------------------------------------------------
void FixupOnlyEntsOccluderEntities()
{
	char str[64];
	int nOccluder = 0;
	for ( entity_num=1; entity_num < num_entities; ++entity_num )
	{
		if (!IsFuncOccluder(entity_num))
			continue;

		// NOTE: If you change the algorithm by which occluder numbers are allocated above,
		// then you must also change this
		sprintf (str, "%i", nOccluder);
		SetKeyValue (&entities[entity_num], "occludernumber", str);
		++nOccluder;
	}
}


void MarkNoDynamicShadowSides()
{
	for ( int iSide=0; iSide < g_MainMap->nummapbrushsides; iSide++ )
	{
		g_MainMap->brushsides[iSide].m_bDynamicShadowsEnabled = true;
	}

	for ( int i=0; i < g_NoDynamicShadowSides.Count(); i++ )
	{
		int brushSideID = g_NoDynamicShadowSides[i];
	
		// Find the side with this ID.
		for ( int iSide=0; iSide < g_MainMap->nummapbrushsides; iSide++ )
		{
			if ( g_MainMap->brushsides[iSide].id == brushSideID )
				g_MainMap->brushsides[iSide].m_bDynamicShadowsEnabled = false;
		}
	}
}

// These must match what is used in engine/networkstringtable.cpp!!!
#define BSPPACK_STRINGTABLE_DICTIONARY_FALLBACK "stringtable_dictionary_fallback.dct"
#define BSPPACK_STRINGTABLE_DICTIONARY_X360_FALLBACK "stringtable_dictionary_fallback_xbox.dct"

#define RESLISTS_FOLDER			"reslists"
#define RESLISTS_FOLDER_X360	"reslists_xbox"

static void AddBufferToPackAndLZMACompress( const char *relativename, void *data, int length )
{
	unsigned int compressedSize = 0;
	byte *compressed = LZMA_Compress( (byte *)data, length, &compressedSize );
	if ( compressed )
	{
		::AddBufferToPak( GetPakFile(), relativename, compressed, compressedSize, false );
		free( compressed );
	}
	else
	{
		::AddBufferToPak( GetPakFile(), relativename, data, length, false );
	}
}

void AddDefaultStringtableDictionaries()
{
	CUtlBuffer buf;
	char reslistsPath[ MAX_PATH ];
	// PC default
	Q_snprintf( reslistsPath, sizeof( reslistsPath ), "%s%s/%s.dict", gamedir, RESLISTS_FOLDER, mapbase );

	// Add PC default file
	if ( g_pFileSystem->ReadFile( reslistsPath, NULL, buf ) )
	{
		// Add to BSP pack file
		::AddBufferToPak( GetPakFile(), BSPPACK_STRINGTABLE_DICTIONARY_FALLBACK, buf.Base(), buf.TellPut(), false );
	}

	buf.Clear();

	// Add 360 default file
	Q_snprintf( reslistsPath, sizeof( reslistsPath ), "%s%s/%s.dict", gamedir, RESLISTS_FOLDER_X360, mapbase );
	if ( g_pFileSystem->ReadFile( reslistsPath, NULL, buf ) )
	{
		// Add to BSP pack file
		::AddBufferToPak( GetPakFile(), BSPPACK_STRINGTABLE_DICTIONARY_X360_FALLBACK, buf.Base(), buf.TellPut(), false );
	}
}

//-----------------------------------------------------------------------------
// Compute the 3D skybox areas
//-----------------------------------------------------------------------------
static void Compute3DSkyboxAreas( node_t *headnode, CUtlVector<int>& areas )
{
	for (int i = 0; i < g_MainMap->num_entities; ++i)
	{
		char* pEntity = ValueForKey(&entities[i], "classname");
		if (!strcmp(pEntity, "sky_camera"))
		{
			// Found a 3D skybox camera, get a leaf that lies in it
			node_t *pLeaf = PointInLeaf( headnode, entities[i].origin );
			if (pLeaf->contents & CONTENTS_SOLID)
			{
				Error ("Error! Entity sky_camera in solid volume! at %.1f %.1f %.1f\n", entities[i].origin.x, entities[i].origin.y, entities[i].origin.z);
			}
			areas.AddToTail( pLeaf->area );
		}
	}
}

bool Is3DSkyboxArea( int area )
{
	for ( int i = g_SkyAreas.Count(); --i >=0; )
	{
		if ( g_SkyAreas[i] == area )
			return true;
	}
	return false;
}

		
/*
============
ProcessModels
============
*/
void ProcessModels (void)
{
	BeginBSPFile ();

	// Mark sides that have no dynamic shadows.
	MarkNoDynamicShadowSides();

	// emit the displacement surfaces
	EmitInitialDispInfos();

	// Clip occluder brushes against each other, 
	// Remove them from the list of models to process below
	EmitOccluderBrushes( );

	for ( entity_num=0; entity_num < num_entities; ++entity_num )
	{
		entity_t *pEntity = &entities[ entity_num ];
		if ( !pEntity->numbrushes )
			continue;

		qprintf ("############### model %i ###############\n", nummodels);

		BeginModel ();

		if (entity_num == 0)
		{
			ProcessWorldModel();
		}
		else
		{
			ProcessSubModel( );
		}

		EndModel ();

		if (!verboseentities)
		{
			verbose = false;	// don't bother printing submodels
		}
	}

	GetMapDataFilesMgr()->AddAllRegisteredFilesToPak();

	// Turn the skybox into a cubemap in case we don't build env_cubemap textures.
	Cubemap_CreateDefaultCubemaps();
	EndBSPFile ();
}


void LoadPhysicsDLL( void )
{
	PhysicsDLLPath( "vphysics.dll" );
}


void PrintCommandLine( int argc, char **argv )
{
	Warning( "Command line: " );
	for ( int z=0; z < argc; z++ )
	{
		Warning( "\"%s\" ", argv[z] );
	}
	Warning( "\n\n" );
}


int RunVBSP( int argc, char **argv )
{
	int		i;
	double		start, end;
	char		path[1024];

	CommandLine()->CreateCmdLine( argc, argv );
	MathLib_Init( 2.2f, 2.2f, 0.0f, OVERBRIGHT, false, false, false, false );
	InstallSpewFunction();
	LoggingSystem_SetChannelSpewLevelByTag( "Developer", LS_MESSAGE );
	
	CmdLib_InitFileSystem( argv[ argc-1 ] );

	Q_StripExtension( ExpandArg( argv[ argc-1 ] ), source, sizeof( source ) );
	Q_FileBase( source, mapbase, sizeof( mapbase ) );
	strlwr( mapbase );

	LoadCmdLineFromFile( argc, argv, mapbase, "vbsp" );

	Msg( "Valve Software - vbsp.exe (%s)\n", __DATE__ );

	for (i=1 ; i<argc ; i++)
	{
		if (!stricmp(argv[i],"-threads"))
		{
			numthreads = atoi (argv[i+1]);
			i++;
		}
		else if (!Q_stricmp(argv[i],"-glview"))
		{
			glview = true;
		}
		else if ( !Q_stricmp(argv[i], "-v") || !Q_stricmp(argv[i], "-verbose") )
		{
			Msg("verbose = true\n");
			verbose = true;
		}
		else if (!Q_stricmp(argv[i], "-noweld"))
		{
			Msg ("noweld = true\n");
			noweld = true;
		}
		else if (!Q_stricmp(argv[i], "-nocsg"))
		{
			Msg ("nocsg = true\n");
			nocsg = true;
		}
		else if (!Q_stricmp(argv[i], "-noshare"))
		{
			Msg ("noshare = true\n");
			noshare = true;
		}
		else if (!Q_stricmp(argv[i], "-notjunc"))
		{
			Msg ("notjunc = true\n");
			notjunc = true;
		}
		else if (!Q_stricmp(argv[i], "-nowater"))
		{
			Msg ("nowater = true\n");
			nowater = true;
		}
		else if (!Q_stricmp(argv[i], "-staticpropcombine"))
		{
			Msg ("staticpropcombine = true\n");
			staticpropcombine = true;
		}
		else if (!Q_stricmp(argv[i], "-keepsources"))
		{
			Msg ("keepsources = true\n");
			staticpropcombine_delsources = false;
		}
		else if ( !Q_stricmp( argv[ i ], "-staticpropcombine_considervis" ) )
		{
			Msg( "staticpropcombine_considervis = true\n" );
			staticpropcombine_considervis = true;
		}
		else if ( !Q_stricmp( argv[ i ], "-staticpropcombine_autocombine" ) )
		{
			Msg( "staticpropcombine_autocombine = true\n" );
			staticpropcombine_autocombine = true;
		}
		else if ( !Q_stricmp( argv[ i ], "-staticpropcombine_suggestrules" ) )
		{
			Msg( "staticpropcombine_suggestcombinerules = true\n" );
			staticpropcombine_suggestcombinerules = true;
		}
		else if ( !Q_stricmp( argv[ i ], "-staticpropcombine_mininstances" ) )
		{
			g_nAutoCombineMinInstances = atoi( argv[ i + 1 ] );
			g_nAutoCombineMinInstances = Max( g_nAutoCombineMinInstances, 2 );
			Msg( "staticpropcombine_mininstances = %d\n", g_nAutoCombineMinInstances );
			i++;
		}

		else if (!Q_stricmp(argv[i], "-combineignore_normals"))				{ Msg ("combineignore_normals = true\n");				staticpropcombine_doflagcompare_STATIC_PROP_IGNORE_NORMALS = false; }
		else if (!Q_stricmp(argv[i], "-combineignore_noshadow"))			{ Msg ("combineignore_noshadow = true\n");				staticpropcombine_doflagcompare_STATIC_PROP_NO_SHADOW = false; }
		else if (!Q_stricmp(argv[i], "-combineignore_noflashlight"))		{ Msg ("combineignore_noflashlight = true\n");			staticpropcombine_doflagcompare_STATIC_PROP_NO_FLASHLIGHT = false; }
		else if (!Q_stricmp(argv[i], "-combineignore_fastreflection"))		{ Msg ("combineignore_fastreflection = true\n");		staticpropcombine_doflagcompare_STATIC_PROP_MARKED_FOR_FAST_REFLECTION = false; }
		else if (!Q_stricmp(argv[i], "-combineignore_novertexlighting"))	{ Msg ("combineignore_novertexlighting = true\n");		staticpropcombine_doflagcompare_STATIC_PROP_NO_PER_VERTEX_LIGHTING = false; }
		else if (!Q_stricmp(argv[i], "-combineignore_noselfshadowing"))		{ Msg ("combineignore_noselfshadowing = true\n");		staticpropcombine_doflagcompare_STATIC_PROP_NO_SELF_SHADOWING = false; }
		else if (!Q_stricmp(argv[i], "-combineignore_disableshadowdepth"))	{ Msg ("combineignore_disableshadowdepth = true\n");	staticpropcombine_doflagcompare_STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH = false; }

		else if (!Q_stricmp(argv[i], "-noopt"))
		{
			Msg ("noopt = true\n");
			noopt = true;
		}
		else if (!Q_stricmp(argv[i], "-noprune"))
		{
			Msg ("noprune = true\n");
			noprune = true;
		}
		else if (!Q_stricmp(argv[i], "-nomerge"))
		{
			Msg ("nomerge = true\n");
			nomerge = true;
		}
		else if (!Q_stricmp(argv[i], "-nomergewater"))
		{
			Msg ("nomergewater = true\n");
			nomergewater = true;
		}
		else if (!Q_stricmp(argv[i], "-nosubdiv"))
		{
			Msg ("nosubdiv = true\n");
			nosubdiv = true;
		}
		else if (!Q_stricmp(argv[i], "-nodetail"))
		{
			Msg ("nodetail = true\n");
			nodetail = true;
		}
		else if (!Q_stricmp(argv[i], "-fulldetail"))
		{
			Msg ("fulldetail = true\n");
			fulldetail = true;
		}
		else if (!Q_stricmp(argv[i], "-alldetail"))
		{
			g_bConvertStructureToDetail = true;
		}
		else if (!Q_stricmp(argv[i], "-onlyents"))
		{
			Msg ("onlyents = true\n");
			onlyents = true;
		}
		else if (!Q_stricmp(argv[i], "-onlyprops"))
		{
			Msg ("onlyprops = true\n");
			onlyprops = true;
		}
		else if (!Q_stricmp(argv[i], "-micro"))
		{
			microvolume = atof(argv[i+1]);
			Msg ("microvolume = %f\n", microvolume);
			i++;
		}
		else if (!Q_stricmp(argv[i], "-leaktest"))
		{
			Msg ("leaktest = true\n");
			leaktest = true;
		}
		else if (!Q_stricmp(argv[i], "-verboseentities"))
		{
			Msg ("verboseentities = true\n");
			verboseentities = true;
		}
		else if (!Q_stricmp(argv[i], "-snapaxial"))
		{
			Msg ("snap axial = true\n");
			g_snapAxialPlanes = true;
		}
#if 0
		else if (!Q_stricmp(argv[i], "-maxlightmapdim"))
		{
			g_maxLightmapDimension = atof(argv[i+1]);
			Msg ("g_maxLightmapDimension = %f\n", g_maxLightmapDimension);
			i++;
		}
#endif
		else if (!Q_stricmp(argv[i], "-block"))
		{
			block_xl = block_xh = atoi(argv[i+1]);
			block_yl = block_yh = atoi(argv[i+2]);
			Msg ("block: %i,%i\n", block_xl, block_yl);
			i+=2;
		}
		else if (!Q_stricmp(argv[i], "-blocks"))
		{
			block_xl = atoi(argv[i+1]);
			block_yl = atoi(argv[i+2]);
			block_xh = atoi(argv[i+3]);
			block_yh = atoi(argv[i+4]);
			Msg ("blocks: %i,%i to %i,%i\n", 
				block_xl, block_yl, block_xh, block_yh);
			i+=4;
		}
		else if (!Q_stricmp(argv[i], "-visgranularity"))
		{
			g_nVisGranularityX = abs( atoi( argv[i+1] ) );
			g_nVisGranularityY = abs( atoi( argv[i+2] ) );
			g_nVisGranularityZ = abs( atoi( argv[i+3] ) );
			Msg ("visgranularity: %i,%i,%i\n", 
				g_nVisGranularityX, g_nVisGranularityY, g_nVisGranularityZ);
			i += 3;
		}
		else if (!Q_stricmp(argv[i], "-blocksize"))
		{
			g_nBlockSize = atoi(argv[i+1]);
			i++;
		}
		else if ( !Q_stricmp( argv[i], "-dumpcollide" ) )
		{
			Msg("Dumping collision models to collideXXX.txt\n" );
			dumpcollide = true;
		}
		else if ( !Q_stricmp( argv[i], "-dumpstaticprop" ) )
		{
			Msg("Dumping static props to staticpropXXX.txt\n" );
			g_DumpStaticProps = true;
		}
		else if ( !Q_stricmp( argv[i], "-forceskyvis" ) )
		{
			Msg("Enabled vis in 3d skybox\n" );
			g_bSkyVis = true;
		}
		else if (!Q_stricmp (argv[i],"-tmpout"))
		{
			strcpy (outbase, "/tmp");
		}
#if 0
		else if( !Q_stricmp( argv[i], "-defaultluxelsize" ) )
		{
			g_defaultLuxelSize = atof( argv[i+1] );
			i++;
		}
#endif
		else if( !Q_stricmp( argv[i], "-luxelscale" ) )
		{
			g_luxelScale = atof( argv[i+1] );
			i++;
		}
		else if( !strcmp( argv[i], "-minluxelscale" ) )
		{
			g_minLuxelScale = atof( argv[i+1] );
			if (g_minLuxelScale < 1)
				g_minLuxelScale = 1;
			i++;
		}
		else if( !strcmp( argv[i], "-maxluxelscale" ) )
		{
			g_maxLuxelScale = atof( argv[i+1] );
			if ( g_maxLuxelScale < 1 )
				g_maxLuxelScale = 1;
			i++;
		}
		else if( !Q_stricmp( argv[i], "-bumpall" ) )
		{
			g_BumpAll = true;
		}
		else if( !Q_stricmp( argv[i], "-low" ) )
		{
			g_bLowPriority = true;
		}
		else if( !Q_stricmp( argv[i], "-lightifmissing" ) )
		{
			g_bLightIfMissing = true;
		}
		else if ( !Q_stricmp( argv[i], CMDLINEOPTION_NOVCONFIG ) )
		{
		}
		else if ( !Q_stricmp( argv[i], "-allowdebug" ) || !Q_stricmp( argv[i], "-steam" ) )
		{
			// nothing to do here, but don't bail on this option
		}
		else if ( !Q_stricmp( argv[i], "-vproject" ) || !Q_stricmp( argv[i], "-game" ) )
		{
			++i;
		}
		else if ( !Q_stricmp( argv[i], "-keepstalezip" ) )
		{
			g_bKeepStaleZip = true;
		}
		else if ( !Q_stricmp( argv[i], "-xbox" ) )
		{
			// enable mandatory xbox extensions
			g_NodrawTriggers = true;
			g_DisableWaterLighting = true;
		}
		else if ( !Q_stricmp( argv[i], "-allowdetailcracks"))
		{
			g_bAllowDetailCracks = true;
		}
		else if ( !Q_stricmp( argv[i], "-novirtualmesh"))
		{
			g_bNoVirtualMesh = true;
		}
		else if ( !Q_stricmp( argv[i], "-replacematerials" ) )
		{
			g_ReplaceMaterials = true;
		}
		else if ( !Q_stricmp(argv[i], "-nodrawtriggers") )
		{
			g_NodrawTriggers = true;
		}
		else if ( !Q_stricmp( argv[i], "-FullMinidumps" ) )
		{
			EnableFullMinidumps( true );
		}
		else if ( !Q_stricmp( argv[i], "-tempcontent" ) )
		{
			// ... Do nothing, just let this pass to the filesystem
		}
		else if ( !Q_stricmp( argv[ i ], "-processheap" ) )
		{
			// ... Do nothing, just let this pass to the mem system
		}
		else if (argv[i][0] == '-')
		{
			Warning("VBSP: Unknown option \"%s\"\n\n", argv[i]);
			i = 100000;	// force it to print the usage
			break;
		}
		else
			break;
	}

	if (i != argc - 1)
	{
		PrintCommandLine( argc, argv );

		Warning(	
			"usage  : vbsp [options...] mapfile\n"
			"example: vbsp -onlyents c:\\hl2\\hl2\\maps\\test\n"
			"\n"
			"Common options (use -v to see all options):\n"
			"\n"
			"  -v (or -verbose): Turn on verbose output (also shows more command\n"
			"                    line options).\n"
			"\n"
			"  -onlyents   : This option causes vbsp only import the entities from the .vmf\n"
			"                file. -onlyents won't reimport brush models.\n"
			"  -onlyprops  : Only update the static props and detail props.\n"
			"  -glview     : Writes .gl files in the current directory that can be viewed\n"
			"                with glview.exe. If you use -tmpout, it will write the files\n"
			"                into the \\tmp folder.\n"
			"  -nodetail   : Get rid of all detail geometry. The geometry left over is\n"
			"                what affects visibility.\n"
			"  -nowater    : Get rid of water brushes.\n"
			"  -staticpropcombine    : Cluster specially supported static prop models.\n"
			"  -keepsources    : Don't clean up cluster models after bspzip.\n"
			"  -staticpropcombine_considervis : Cluster static prop models only within\n"
			"                                   vis clusters.\n"
			"  -staticpropcombine_autocombine : Automatically combine simple static props\n"
			"                                   without an explicit combine rule.\n"
			"  -staticpropcombine_suggestrules: Suggest rules to add to spcombinerules.txt\n"
			"  -low        : Run as an idle-priority process.\n"
			"\n"
			"  -vproject <directory> : Override the VPROJECT environment variable.\n"
			"  -game <directory>     : Same as -vproject.\n"
			"\n" );

		if ( verbose )
		{
			Warning(
				"Other options  :\n"
				"  -novconfig   : Don't bring up graphical UI on vproject errors.\n"
				"  -threads     : Control the number of threads vbsp uses (defaults to the # of\n"
				"                 processors on your machine).\n"
				"  -verboseentities: If -v is on, this disables verbose output for submodels.\n"
				"  -noweld      : Don't join face vertices together.\n"
				"  -nocsg       : Don't chop out intersecting brush areas.\n"
				"  -noshare     : Emit unique face edges instead of sharing them.\n"
				"  -notjunc     : Don't fixup t-junctions.\n"
				"  -noopt       : By default, vbsp removes the 'outer shell' of the map, which\n"
				"                 are all the faces you can't see because you can never get\n"
				"                 outside the map. -noopt disables this behaviour.\n"
				"  -noprune     : Don't prune neighboring solid nodes.\n"
				"  -nomerge     : Don't merge together chopped faces on nodes.\n"
				"  -nomergewater: Don't merge together chopped faces on water.\n"
				"  -nosubdiv    : Don't subdivide faces for lightmapping.\n"
				"  -micro <#>   : vbsp will warn when brushes are output with a volume less\n"
				"                 than this number (default: 1.0).\n"
				"  -fulldetail  : Mark all detail geometry as normal geometry (so all detail\n"
				"                 geometry will affect visibility).\n" 
				"  -alldetail   : Convert all structural brushes to detail brushes, except\n"
				"                 func_brush entities whose names begin with ""structure_"".\n"
				);
			Warning(
				"  -leaktest    : Stop processing the map if a leak is detected. Whether or not\n"
				"                 this flag is set, a leak file will be written out at\n"
				"                 <vmf filename>.lin, and it can be imported into Hammer.\n"
				"  -bumpall     : Force all surfaces to be bump mapped.\n"
				"  -snapaxial   : Snap axial planes to integer coordinates.\n"
				"  -block # #      : Control the grid size mins that vbsp chops the level on.\n"
				"  -blocks # # # # : Enter the mins and maxs for the grid size vbsp uses.\n"
				"  -blocksize #    : Control the size of each grid square that vbsp chops the level on.  Default is 1024."
				"  -dumpstaticprops: Dump static props to staticprop*.txt\n"
				"  -dumpcollide    : Write files with collision info.\n"
				"  -forceskyvis	   : Enable vis calculations in 3d skybox leaves\n"
				"  -luxelscale #   : Scale all lightmaps by this amount (default: 1.0).\n"
				"  -minluxelscale #: No luxel scale will be lower than this amount (default: 1.0).\n"
				"  -maxluxelscale #: No luxel scale will be higher than this amount (default: 999999.0).\n"
				"  -lightifmissing : Force lightmaps to be generated for all surfaces even if\n"
				"                    they don't need lightmaps.\n"
				"  -keepstalezip   : Keep the BSP's zip files intact but regenerate everything\n"
				"                    else.\n"
				"  -virtualdispphysics : Use virtual (not precomputed) displacement collision\n"
				"						 models\n"
				"  -visgranularity # # # : Force visibility splits # of units along X, Y, Z\n"
				"  -xbox		: Enable mandatory xbox options\n"
				"  -x360		: Generate Xbox360 version of vsp\n"
				"  -nox360		: Disable generation Xbox360 version of vsp (default)\n"
				"  -replacematerials : Substitute materials according to materialsub.txt in\n"
				"					   content\\maps\n"
				"  -FullMinidumps	: Write large minidumps on crash.\n"
				);
			}

		DeleteCmdLine( argc, argv );
		CmdLib_Cleanup();
		exit( 1 );
	}

	start = Plat_FloatTime();

	// Run in the background?
	if( g_bLowPriority )
	{
		SetLowPriority();
	}

	ThreadSetDefault ();
	numthreads = 1;		// multiple threads aren't helping...

	// Setup the logfile.
	char logFile[512];
	_snprintf( logFile, sizeof(logFile), "%s.log", source );
	g_CmdLibFileLoggingListener.Open( logFile );

	LoadPhysicsDLL();
	LoadSurfaceProperties();

#if 0
	Msg( "qdir: %s  This is the the path of the initial source file \n", qdir );
	Msg( "gamedir: %s This is the base engine + mod-specific game dir (e.g. d:/tf2/mytfmod/) \n", gamedir );
	Msg( "basegamedir: %s This is the base engine + base game directory (e.g. e:/hl2/hl2/, or d:/tf2/tf2/ )\n", basegamedir );
#endif

	sprintf( materialPath, "%smaterials", gamedir );
	InitMaterialSystem( materialPath, CmdLib_GetFileSystemFactory() );
	Msg( "materialPath: %s\n", materialPath );
	
	// delete portal and line files
	sprintf (path, "%s.prt", source);
	remove (path);
	sprintf (path, "%s.lin", source);
	remove (path);

	strcpy (name, ExpandArg (argv[i]));	

	const char *pszExtension = V_GetFileExtension( name );
	if ( !pszExtension )
	{
		V_SetExtension( name, ".vmm", sizeof( name ) );
		if ( !FileExists( name ) )
		{
			V_SetExtension( name, ".vmf", sizeof( name ) );
		}
	}

	char platformBSPFileName[1024];
	GetPlatformMapPath( source, platformBSPFileName, 0, 1024 );
	
	// if we're combining materials, load the script file
	if ( g_ReplaceMaterials )
	{
		LoadMaterialReplacementKeys( gamedir, mapbase );
	}

	//
	// if onlyents, just grab the entites and resave
	//
	if (onlyents)
	{
		LoadBSPFile (platformBSPFileName);
		num_entities = 0;
		// Clear out the cubemap samples since they will be reparsed even with -onlyents
		g_nCubemapSamples = 0;

		// Mark as stale since the lighting could be screwed with new ents.
		AddBufferToPak( GetPakFile(), "stale.txt", "stale", strlen( "stale" ) + 1, false );

		LoadMapFile (name);
		SetModelNumbers ();
		SetLightStyles ();

		// NOTE: If we ever precompute lighting for static props in
		// vrad, EmitStaticProps should be removed here

		// Emit static props found in the .vmf file
		EmitStaticProps();

		// NOTE: Don't deal with detail props here, it blows away lighting

		// Recompute the skybox
		ComputeBoundsNoSkybox();

		// Make sure that we have a water lod control eneity if we have water in the map.
		EnsurePresenceOfWaterLODControlEntity();

		// Make sure the func_occluders have the appropriate data set
		FixupOnlyEntsOccluderEntities();

		// Doing this here because stuff abov may filter out entities
		UnparseEntities ();

		WriteBSPFile (platformBSPFileName);
	}
	else if (onlyprops)
	{
		// In the only props case, deal with static + detail props only
		LoadBSPFile (platformBSPFileName);

		LoadMapFile(name);
		SetModelNumbers();
		SetLightStyles();

		// Emit static props found in the .vmf file
		EmitStaticProps();

		// Place detail props found in .vmf and based on material properties
		LoadEmitDetailObjectDictionary( gamedir );
		EmitDetailObjects();

		WriteBSPFile (platformBSPFileName);
	}
	else
	{
		//
		// start from scratch
		//

		// Load just the file system from the bsp
		if( g_bKeepStaleZip && FileExists( platformBSPFileName ) )
		{
			LoadBSPFile_FileSystemOnly (platformBSPFileName);
			// Mark as stale since the lighting could be screwed with new ents.
			AddBufferToPak( GetPakFile(), "stale.txt", "stale", strlen( "stale" ) + 1, false );
		}

		LoadMapFile (name);

		InsertVisibilitySplittingHintBrushes();

		WorldVertexTransitionFixup();

		Cubemap_FixupBrushSidesMaterials();
		Cubemap_AttachDefaultCubemapToSpecularSides();
		Cubemap_AddUnreferencedCubemaps();

		SetModelNumbers ();
		SetLightStyles ();
		LoadEmitDetailObjectDictionary( gamedir );
		AddDefaultStringtableDictionaries();
		ProcessModels ();
	}

	end = Plat_FloatTime();
	
	char str[512];
	GetHourMinuteSecondsString( (int)( end - start ), str, sizeof( str ) );
	Msg( "%s elapsed\n", str );

	DeleteCmdLine( argc, argv );
	ReleasePakFileLumps();
	DeleteMaterialReplacementKeys();
	ShutdownMaterialSystem();
	CmdLib_Cleanup();
	return 0;
}


/*
=============
main
============
*/
int main (int argc, char **argv)
{
	// Install an exception handler.
	SetupDefaultToolsMinidumpHandler();
	return RunVBSP( argc, argv );
}


