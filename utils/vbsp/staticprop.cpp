//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Places "detail" objects which are client-only renderable things
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "vbsp.h"
#include "bsplib.h"
#include "UtlVector.h"
#include "bspfile.h"
#include "gamebspfile.h"
#include "VPhysics_Interface.h"
#include "Studio.h"
#include "byteswap.h"
#include "UtlBuffer.h"
#include "CollisionUtils.h"
#include <float.h>
#include "CModel.h"
#include "PhysDll.h"
#include "UtlSymbol.h"
#include "tier1/strtools.h"
#include "keyvalues.h"
#include "map.h"
#include "tier3/tier3.h"
#include "phyfile.h"
#include "characterset.h"
#include "utlstring.h"

#ifdef IS_WINDOWS_PC
#include "winlite.h"
#endif


#define STATIC_PROP_COMBINE_ENABLED


static void SetCurrentModel( studiohdr_t *pStudioHdr );
static void FreeCurrentModelVertexes();

IPhysicsCollision *s_pPhysCollision = NULL;

//-----------------------------------------------------------------------------
// These puppies are used to construct the game lumps
//-----------------------------------------------------------------------------
static CUtlVector<StaticPropDictLump_t>	s_StaticPropDictLump;
static CUtlVector<StaticPropLump_t>		s_StaticPropLump;
static CUtlVector<StaticPropLeafLump_t>	s_StaticPropLeafLump;


//-----------------------------------------------------------------------------
// Used to build the static prop
//-----------------------------------------------------------------------------
struct StaticPropBuild_t
{
	char const* m_pModelName;
	char const* m_pLightingOrigin;
	Vector	m_Origin;
	QAngle	m_Angles;
	int		m_Solid;
	int		m_Skin;
	int		m_Flags;
	int		m_FlagsEx;
	float	m_FadeMinDist;
	float	m_FadeMaxDist;
	bool	m_FadesOut;
	float	m_flForcedFadeScale;
	unsigned char	m_nMinCPULevel;
	unsigned char	m_nMaxCPULevel;
	unsigned char	m_nMinGPULevel;
	unsigned char	m_nMaxGPULevel;
	color32 m_DiffuseModulation;

	bool		m_bCombineDataWritten;
	int			m_nPhysicsHullCount;
	CUtlString	m_szRefName;
	CUtlString	m_szPhyName;
	int			m_nHulls;
	bool		m_bConcave;
	float		m_flScale;
	int			m_nCombineRuleGroup;
	bool		m_bUpaxisY;
};
 

//-----------------------------------------------------------------------------
// Used to cache collision model generation
//-----------------------------------------------------------------------------
struct ModelCollisionLookup_t
{
	CUtlSymbol m_Name;
	CPhysCollide* m_pCollide;
};

static bool ModelLess( ModelCollisionLookup_t const& src1, ModelCollisionLookup_t const& src2 )
{
	return src1.m_Name < src2.m_Name;
}

static CUtlRBTree<ModelCollisionLookup_t, unsigned short>	s_ModelCollisionCache( 0, 32, ModelLess );
static CUtlVector<int>	s_LightingInfo;


//-----------------------------------------------------------------------------
// Gets the keyvalues from a studiohdr
//-----------------------------------------------------------------------------
bool StudioKeyValues( studiohdr_t* pStudioHdr, KeyValues *pValue )
{
	if ( !pStudioHdr )
		return false;

	return pValue->LoadFromBuffer( pStudioHdr->pszName(), pStudioHdr->KeyValueText() );
}


//-----------------------------------------------------------------------------
// Makes sure the studio model is a static prop
//-----------------------------------------------------------------------------
enum isstaticprop_ret
{
	RET_VALID,
	RET_FAIL_NOT_MARKED_STATIC_PROP,
	RET_FAIL_DYNAMIC,
};

isstaticprop_ret IsStaticProp( studiohdr_t* pHdr )
{
	if (!(pHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP))
		return RET_FAIL_NOT_MARKED_STATIC_PROP;

	// If it's got a propdata section in the model's keyvalues, it's not allowed to be a prop_static
	KeyValues *modelKeyValues = new KeyValues(pHdr->pszName());
	if ( StudioKeyValues( pHdr, modelKeyValues ) )
	{
		KeyValues *sub = modelKeyValues->FindKey("prop_data");
		if ( sub )
		{
			if ( !(sub->GetInt( "allowstatic", 0 )) )
			{
				modelKeyValues->deleteThis();
				return RET_FAIL_DYNAMIC;
			}
		}
	}
	modelKeyValues->deleteThis();

	return RET_VALID;
}


//-----------------------------------------------------------------------------
// Add static prop model to the list of models
//-----------------------------------------------------------------------------

static int AddStaticPropDictLump( char const* pModelName )
{
	StaticPropDictLump_t dictLump;
	strncpy( dictLump.m_Name, pModelName, DETAIL_NAME_LENGTH );

	for (int i = s_StaticPropDictLump.Count(); --i >= 0; )
	{
		if (!memcmp(&s_StaticPropDictLump[i], &dictLump, sizeof(dictLump) ))
			return i;
	}

	return s_StaticPropDictLump.AddToTail( dictLump );
}


//-----------------------------------------------------------------------------
// Load studio model vertex data from a file...
//-----------------------------------------------------------------------------
bool LoadStudioModel( char const* pModelName, char const* pEntityType, CUtlBuffer& buf )
{
	if ( !GetMapDataFilesMgr()->ReadRegisteredFile( pModelName, buf ) &&
		 !g_pFullFileSystem->ReadFile( pModelName, NULL, buf ) )
	{
		if ( V_stristr( pModelName, "_autocombine_" ) && FileExistsInPak( GetPakFile(), pModelName ) )
		{
			if ( !ReadFileFromPak( GetPakFile(), pModelName, false, buf ) )
				return false;
		}
		else
		{
			return false;
		}
	}

	// Check that it's valid
	if (strncmp ((const char *) buf.PeekGet(), "IDST", 4) &&
		strncmp ((const char *) buf.PeekGet(), "IDAG", 4))
	{
		return false;
	}

	studiohdr_t* pHdr = (studiohdr_t*)buf.PeekGet();

	Studio_ConvertStudioHdrToNewVersion( pHdr );

	if (pHdr->version != STUDIO_VERSION)
	{
		return false;
	}

	isstaticprop_ret isStaticProp = IsStaticProp(pHdr);
	if ( isStaticProp != RET_VALID )
	{
		if ( isStaticProp == RET_FAIL_NOT_MARKED_STATIC_PROP )
		{
			Warning("Error! To use model \"%s\"\n"
				"      with %s, it must be compiled with $staticprop!\n", pModelName, pEntityType );
		}
		else if ( isStaticProp == RET_FAIL_DYNAMIC )
		{
			Warning("Error! %s using model \"%s\", which must be used on a dynamic entity (i.e. prop_physics). Deleted.\n", pEntityType, pModelName );
		}
		return false;
	}

	// ensure reset
	pHdr->SetVertexBase( NULL );
	pHdr->SetIndexBase( NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Computes a convex hull from a studio mesh
//-----------------------------------------------------------------------------
static CPhysConvex* ComputeConvexHull( studiohdr_t* pStudioHdr, mstudiomesh_t* pMesh )
{
	// Generate a list of all verts in the mesh
	CUtlVector<Vector> vertCopy;
	CUtlVector<Vector *> ppVerts;
	vertCopy.EnsureCount(pMesh->numvertices);
	ppVerts.EnsureCount(pMesh->numvertices);
	const mstudio_meshvertexdata_t *vertData = pMesh->GetVertexData( (void *)pStudioHdr );
	Assert( vertData ); // This can only return NULL on X360 for now
	for (int i = 0; i < pMesh->numvertices; ++i)
	{
		vertCopy[i] = *vertData->Position(i); 
		// quantize these so that really curved/detailed models don't take forever
		vertCopy[i].x = float( RoundFloatToInt(vertCopy[i].x) );
		vertCopy[i].y = float( RoundFloatToInt(vertCopy[i].y) );
		vertCopy[i].z = float( RoundFloatToInt(vertCopy[i].z) );
		ppVerts[i] = &vertCopy[i];
	}

	// Generate a convex hull from the verts
	return s_pPhysCollision->ConvexFromVerts( ppVerts.Base(), pMesh->numvertices );
}


//-----------------------------------------------------------------------------
// Computes a convex hull from the studio model
//-----------------------------------------------------------------------------
CPhysCollide* ComputeConvexHull( studiohdr_t* pStudioHdr )
{
	CUtlVector<CPhysConvex*>	convexHulls;

	for (int body = 0; body < pStudioHdr->numbodyparts; ++body )
	{
		mstudiobodyparts_t *pBodyPart = pStudioHdr->pBodypart( body );
		for( int model = 0; model < pBodyPart->nummodels; ++model )
		{
			mstudiomodel_t *pStudioModel = pBodyPart->pModel( model );
			for( int mesh = 0; mesh < pStudioModel->nummeshes; ++mesh )
			{
				// Make a convex hull for each mesh
				// NOTE: This won't work unless the model has been compiled
				// with $staticprop
				mstudiomesh_t *pStudioMesh = pStudioModel->pMesh( mesh );
				CPhysConvex *pConvex = ComputeConvexHull( pStudioHdr, pStudioMesh );
				if ( !pConvex )
				{
					Warning("Can't create hull for mesh %d/%d of model %s\n", mesh, model, pStudioHdr->name );
				}
				else
				{
					convexHulls.AddToTail( pConvex );
				}
			}
		}
	}

	// Convert an array of convex elements to a compiled collision model
	// (this deletes the convex elements)
	return s_pPhysCollision->ConvertConvexToCollide( convexHulls.Base(), convexHulls.Count() );
}


//-----------------------------------------------------------------------------
// Add, find collision model in cache
//-----------------------------------------------------------------------------
static CPhysCollide* GetCollisionModel( char const* pModelName )
{
	// Convert to a common string
	char* pTemp = (char*)_alloca(strlen(pModelName) + 1);
	strcpy( pTemp, pModelName );
	_strlwr( pTemp );

	char* pSlash = strchr( pTemp, '\\' );
	while( pSlash )
	{
		*pSlash = '/';
		pSlash = strchr( pTemp, '\\' );
	}

	// Find it in the cache
	ModelCollisionLookup_t lookup;
	lookup.m_Name = pTemp;
	int i = s_ModelCollisionCache.Find( lookup );
	if (i != s_ModelCollisionCache.InvalidIndex())
		return s_ModelCollisionCache[i].m_pCollide;

	// Load the studio model file
	CUtlBuffer buf;
	if (!LoadStudioModel(pModelName, "prop_static", buf))
	{
		Warning("Error loading studio model \"%s\"!\n", pModelName );

		// This way we don't try to load it multiple times
		lookup.m_pCollide = 0;
		s_ModelCollisionCache.Insert( lookup );

		return 0;
	}

	// Compute the convex hull of the model...
	studiohdr_t* pStudioHdr = (studiohdr_t*)buf.PeekGet();

	// necessary for vertex access
	SetCurrentModel( pStudioHdr );

	lookup.m_pCollide = ComputeConvexHull( pStudioHdr );
	s_ModelCollisionCache.Insert( lookup );

	if ( !lookup.m_pCollide )
	{
		Warning("Bad geometry on \"%s\"!\n", pModelName );
	}

	// Debugging
	if (g_DumpStaticProps)
	{
		static int propNum = 0;
		char tmp[128];
		sprintf( tmp, "staticprop%03d.txt", propNum );
		DumpCollideToGlView( lookup.m_pCollide, tmp );
		++propNum;
	}

	FreeCurrentModelVertexes();

	// Insert into cache...
	return lookup.m_pCollide;
}


//-----------------------------------------------------------------------------
// Tests a single leaf against the static prop
//-----------------------------------------------------------------------------

static bool TestLeafAgainstCollide( int depth, int* pNodeList, 
	Vector const& origin, QAngle const& angles, CPhysCollide* pCollide )
{
	// Copy the planes in the node list into a list of planes
	float* pPlanes = (float*)_alloca(depth * 4 * sizeof(float) );
	int idx = 0;
	for (int i = depth; --i >= 0; ++idx )
	{
		int sign = (pNodeList[i] < 0) ? -1 : 1;
		int node = (sign < 0) ? - pNodeList[i] - 1 : pNodeList[i];
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		pPlanes[idx*4] = sign * pPlane->normal[0];
		pPlanes[idx*4+1] = sign * pPlane->normal[1];
		pPlanes[idx*4+2] = sign * pPlane->normal[2];
		pPlanes[idx*4+3] = sign * pPlane->dist;
	}

	// Make a convex solid out of the planes
	CPhysConvex* pPhysConvex = s_pPhysCollision->ConvexFromPlanes( pPlanes, depth, 0.0f );

	// This should never happen, but if it does, return no collision
	Assert( pPhysConvex );
	if (!pPhysConvex)
		return false;

	CPhysCollide* pLeafCollide = s_pPhysCollision->ConvertConvexToCollide( &pPhysConvex, 1 );

	// Collide the leaf solid with the static prop solid
	trace_t	tr;
	s_pPhysCollision->TraceCollide( vec3_origin, vec3_origin, pLeafCollide, vec3_angle,
		pCollide, origin, angles, &tr );

	s_pPhysCollision->DestroyCollide( pLeafCollide );

	return (tr.startsolid != 0);
}

//-----------------------------------------------------------------------------
// Find all leaves that intersect with this bbox + test against the static prop..
//-----------------------------------------------------------------------------

static void ComputeConvexHullLeaves_R( int node, int depth, int* pNodeList,
	Vector const& mins, Vector const& maxs,
	Vector const& origin, QAngle const& angles,	CPhysCollide* pCollide, bool bSkipTrace,
	CUtlVector<unsigned short>& leafList )
{
	Assert( pNodeList && pCollide );
	Vector cornermin, cornermax;

	while( node >= 0 )
	{
		dnode_t* pNode = &dnodes[node];
		dplane_t* pPlane = &dplanes[pNode->planenum];

		// Arbitrary split plane here
		for (int i = 0; i < 3; ++i)
		{
			if (pPlane->normal[i] >= 0)
			{
				cornermin[i] = mins[i];
				cornermax[i] = maxs[i];
			}
			else
			{
				cornermin[i] = maxs[i];
				cornermax[i] = mins[i];
			}
		}

		if (DotProduct( pPlane->normal, cornermax ) <= pPlane->dist)
		{
			// Add the node to the list of nodes
			pNodeList[depth] = node;
			++depth;

			node = pNode->children[1];
		}
		else if (DotProduct( pPlane->normal, cornermin ) >= pPlane->dist)
		{
			// In this case, we are going in front of the plane. That means that
			// this plane must have an outward normal facing in the oppisite direction
			// We indicate this be storing a negative node index in the node list
			pNodeList[depth] = - node - 1;
			++depth;

			node = pNode->children[0];
		}
		else
		{
			// Here the box is split by the node. First, we'll add the plane as if its
			// outward facing normal is in the direction of the node plane, then
			// we'll have to reverse it for the other child...
			pNodeList[depth] = node;
			++depth;

			ComputeConvexHullLeaves_R( pNode->children[1], depth, pNodeList, mins, maxs, origin, angles, pCollide, bSkipTrace, leafList );
			
			pNodeList[depth - 1] = - node - 1;
			ComputeConvexHullLeaves_R( pNode->children[0], depth, pNodeList, mins, maxs, origin, angles, pCollide, bSkipTrace, leafList );
			return;
		}
	}

	Assert( pNodeList && pCollide );

	// Never add static props to solid leaves
	if ( (dleafs[-node-1].contents & CONTENTS_SOLID) == 0 )
	{
		if ( bSkipTrace || TestLeafAgainstCollide( depth, pNodeList, origin, angles, pCollide ) )
		{
			leafList.AddToTail( -node - 1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Places Static Props in the level
//-----------------------------------------------------------------------------

static void ComputeStaticPropLeaves( CPhysCollide* pCollide, Vector const& origin, QAngle const& angles, CUtlVector<unsigned short>& leafList )
{
	// Compute an axis-aligned bounding box for the collide
	Vector mins, maxs;
	s_pPhysCollision->CollideGetAABB( &mins, &maxs, pCollide, origin, angles );

	Vector vSize = maxs - mins;
	bool bSkipTrace = false;
	if ( vSize.x < 1e-2f || vSize.y < 1e-2f || vSize.z < 1e-2f )
	{
		// 2d, enlarge and skip the accurate test
		bSkipTrace = true;
		for ( int i = 0; i < 3; i++ )
		{
			if ( vSize[i] < 1e-2f )
			{
				mins[i] -= 1.0f;
				maxs[i] += 1.0f;
			}
		}
	}
	// Find all leaves that intersect with the bounds
	int tempNodeList[1024];
	ComputeConvexHullLeaves_R( 0, 0, tempNodeList, mins, maxs, origin, angles, pCollide, bSkipTrace, leafList );
}


//-----------------------------------------------------------------------------
// Computes the lighting origin
//-----------------------------------------------------------------------------
static bool ComputeLightingOrigin( StaticPropBuild_t const& build, Vector& lightingOrigin )
{
	for (int i = s_LightingInfo.Count(); --i >= 0; )
	{
		int entIndex = s_LightingInfo[i];

		// Check against all lighting info entities
		char const* pTargetName = ValueForKey( &entities[entIndex], "targetname" );
		if (!Q_strcmp(pTargetName, build.m_pLightingOrigin))
		{
			GetVectorForKey( &entities[entIndex], "origin", lightingOrigin );
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Places Static Props in the level
//-----------------------------------------------------------------------------
static void AddStaticPropToLump( StaticPropBuild_t const& build )
{
	// Get the collision model
	CPhysCollide* pConvexHull = GetCollisionModel( build.m_pModelName );
	if (!pConvexHull)
		return;

	// Compute the leaves the static prop's convex hull hits
	CUtlVector< unsigned short > leafList;
	ComputeStaticPropLeaves( pConvexHull, build.m_Origin, build.m_Angles, leafList );

	if ( !leafList.Count() )
	{
		Warning( "Static prop %s outside the map (%.2f, %.2f, %.2f)\n", build.m_pModelName, build.m_Origin.x, build.m_Origin.y, build.m_Origin.z );
		return;
	}
	// Insert an element into the lump data...
	int i = s_StaticPropLump.AddToTail( );
	StaticPropLump_t& propLump = s_StaticPropLump[i];
	propLump.m_PropType = AddStaticPropDictLump( build.m_pModelName ); 
	VectorCopy( build.m_Origin, propLump.m_Origin );
	VectorCopy( build.m_Angles, propLump.m_Angles );
	propLump.m_FirstLeaf = s_StaticPropLeafLump.Count();
	propLump.m_LeafCount = leafList.Count();
	propLump.m_Solid = build.m_Solid;
	propLump.m_Skin = build.m_Skin;
	propLump.m_Flags = build.m_Flags;
	propLump.m_FlagsEx = build.m_FlagsEx;
	if (build.m_FadesOut)
	{
		propLump.m_Flags |= STATIC_PROP_FLAG_FADES;
	}
	propLump.m_FadeMinDist = build.m_FadeMinDist;
	propLump.m_FadeMaxDist = build.m_FadeMaxDist;
	propLump.m_flForcedFadeScale = build.m_flForcedFadeScale;
	propLump.m_nMinCPULevel = build.m_nMinCPULevel;
	propLump.m_nMaxCPULevel = build.m_nMaxCPULevel;
	propLump.m_nMinGPULevel = build.m_nMinGPULevel;
	propLump.m_nMaxGPULevel = build.m_nMaxGPULevel;
	propLump.m_DiffuseModulation = build.m_DiffuseModulation;
	propLump.m_bDisableX360 = false;	
	
	if (build.m_pLightingOrigin && *build.m_pLightingOrigin)
	{
		if (ComputeLightingOrigin( build, propLump.m_LightingOrigin ))
		{
			propLump.m_Flags |= STATIC_PROP_USE_LIGHTING_ORIGIN;
		}
	}

	// Add the leaves to the leaf lump
	for (int j = 0; j < leafList.Count(); ++j)
	{
		StaticPropLeafLump_t insert;
		insert.m_Leaf = leafList[j];
		s_StaticPropLeafLump.AddToTail( insert );
	}
}


//-----------------------------------------------------------------------------
// Places static props in the lump
//-----------------------------------------------------------------------------

static void SetLumpData( )
{
	GameLumpHandle_t handle = g_GameLumps.GetGameLumpHandle(GAMELUMP_STATIC_PROPS);
	if (handle != g_GameLumps.InvalidGameLump())
		g_GameLumps.DestroyGameLump(handle);

	int dictsize = s_StaticPropDictLump.Count() * sizeof(StaticPropDictLump_t);
	int objsize = s_StaticPropLump.Count() * sizeof(StaticPropLump_t);
	int leafsize = s_StaticPropLeafLump.Count() * sizeof(StaticPropLeafLump_t);
	int size = dictsize + objsize + leafsize + 3 * sizeof(int);

	handle = g_GameLumps.CreateGameLump( GAMELUMP_STATIC_PROPS, size, 0, GAMELUMP_STATIC_PROPS_VERSION );

	// Serialize the data
	CUtlBuffer buf( g_GameLumps.GetGameLump(handle), size );
	buf.PutInt( s_StaticPropDictLump.Count() );
	if (dictsize)
		buf.Put( s_StaticPropDictLump.Base(), dictsize );
	buf.PutInt( s_StaticPropLeafLump.Count() );
	if (leafsize)
		buf.Put( s_StaticPropLeafLump.Base(), leafsize );
	buf.PutInt( s_StaticPropLump.Count() );
	if (objsize)
		buf.Put( s_StaticPropLump.Base(), objsize );
}


//-----------------------------------------------------------------------------
// Places Static Props in the level
//-----------------------------------------------------------------------------

typedef CUtlVector<StaticPropBuild_t> propBuildVector;

KeyValues *kvSPCombineRules = new KeyValues( "SPCombineRules" );

struct staticpropcombinepeer_t
{
	CUtlString m_szMdlName;
	CUtlString m_szRefName;
	CUtlString m_szPhyName;
	CUtlString m_surfaceProp;
	int m_nHulls;
	bool m_bConcave;
	float m_flScale;
	bool m_bUpaxisY;
	//CUtlString m_szConcatTextureNames;
};

struct staticpropcombinerule_t
{
	CUtlString m_szGroupName;
	CUtlString m_szQcTemplatePath;
	CUtlBuffer m_bufAutoGeneratedQCTemplate;
	CCopyableUtlVector<staticpropcombinepeer_t> m_vecCombinePeers;
	int m_nClusterLimit;
	float m_flDistanceLimit;
	CCopyableUtlVector<int> m_vecStatsMemberCounts;

	staticpropcombinerule_t() : m_bufAutoGeneratedQCTemplate(0, 0, CUtlBuffer::TEXT_BUFFER) {}
};

CUtlVector<staticpropcombinerule_t> g_vecCombineRules;

CUtlVector<CUtlString> g_vecGeneratedModelNames;

#define DEFAULT_COMBINE_STATIC_PROP_DISTANCE 129.0f
#define DEFAULT_COMBINE_STATIC_PROP_COUNT 16

#define MAX_HULLS 32
#define MAX_EXTRA_COLLISION_MODELS 24



struct convextriangle_t
{
	int m_nPad;
	short m_nEdges[6];
	int GetVert( int nIndex ) const
	{
		return m_nEdges[ nIndex * 2 ];
	}
};

struct convexleaf_t
{
	int m_nOffsetVerts;
	int m_nPad[2];
	short m_nTriangleCount;
	short m_nUnused;

	const convextriangle_t *GetFirstTriangle() const { return reinterpret_cast<const convextriangle_t *>(this+1); }
	const float *GetVertArray() const { return reinterpret_cast<float *>( ((char *)this) + m_nOffsetVerts); }
};

struct treenode_t
{
	int m_nRightNode;		// if not zero, there are children
	int m_nConvexOffset;	// offset to the convex if this is a leaf
	float m_nFloats[5];
	bool IsLeaf() const { return m_nRightNode == 0 ? true : false; }
	const convexleaf_t *GetConvex() const { return reinterpret_cast<convexleaf_t *>( ((char *)this) + m_nConvexOffset); }
	const treenode_t *GetLeftChild() const { return this + 1; }
	const treenode_t *GetRightChild() const { return reinterpret_cast<const treenode_t *>( ((char *)this) + m_nRightNode ); }
};


struct collisionmodel_t
{
	float m_flVals[7];
	int m_nSurface;
	int m_nOffsetTree;
	int m_nPad[3];
	const treenode_t *GetRoot() const { return reinterpret_cast<const treenode_t *>( ((const char *)this) + m_nOffsetTree); }
};


struct solidheader_t
{
	int		m_nSolidSize;
	int		m_nID;
	short	m_nVersion;
	short	m_nType;
	int		m_nSize;
	float	m_flAreas[3];
	int		m_nAxisMapSize;
	// have to skip 4 more bytes because m_nSolidSize itself is not included in the total
	const solidheader_t *GetNextSolid() const { return reinterpret_cast<const solidheader_t *>( ((const char *)this) + m_nSolidSize + 4); }


	const collisionmodel_t *GetCollisionModel() const { return reinterpret_cast<const collisionmodel_t *>(this+1); }
};

struct phyfileheader_t
{
	int		m_nHeaderSize;
	int		m_nZero;
	int		m_nSolidCount;
	int		m_nCheckSum;
	const solidheader_t *GetFirstSolid() const { return reinterpret_cast<const solidheader_t *>(this+1); }
	int GetCollisionTextOffset() const 
	{
		const solidheader_t *pSolid = GetFirstSolid();
		for ( int i = 0; i < m_nSolidCount; i++ )
		{
			pSolid = pSolid->GetNextSolid();
		}
		return reinterpret_cast<const char *>(pSolid) - reinterpret_cast<const char *>(this);
	}
};

int GetLeaves_r( CUtlVector<const convexleaf_t *> &list, const treenode_t *pNode )
{
	while ( pNode )
	{
		if ( pNode->IsLeaf() )
		{
			list.AddToTail( pNode->GetConvex() );
			return list.Count();
		}
		GetLeaves_r( list, pNode->GetLeftChild() );
		pNode = pNode->GetRightChild();
	}
	return list.Count();

}

bool InitStaticPropCombinePeer( staticpropcombinepeer_t *newPeer )
{
	newPeer->m_nHulls = 0;
	newPeer->m_bConcave = false;
	newPeer->m_bUpaxisY = false;
	newPeer->m_flScale = 1.0f;

	// old method was to manually specify the src ref and phy
	//newPeer->m_szRefName.Set( pPeer->GetString( "ref", "error" ) );
	//newPeer->m_szPhyName.Set( pPeer->GetString( "phy", newPeer->m_szRefName.Get() ) );


	// but if we can get the model qc path out of the mdl, then crudely parse out the sources, we can verify them automatically:

	CUtlBuffer buf;
	if ( !LoadStudioModel( newPeer->m_szMdlName.Get(), "prop_static", buf ) )
	{
		Error( "Error loading studio model \"%s\"!\n", newPeer->m_szMdlName.Get() );
		return false;
	}

	studiohdr_t* pStudioHdr = (studiohdr_t*)buf.PeekGet();

	bool bExtractedSources = false;
	if ( pStudioHdr )
	{
		newPeer->m_surfaceProp = pStudioHdr->pszSurfaceProp();

		//char szConcatTextureNames[256];
		//szConcatTextureNames[0] = 0;
		//if( pStudioHdr->textureindex != 0 )
		//{
		//	//Msg( "%i textures in %s\n", pStudioHdr->numtextures, newPeer->m_szMdlName.Get() );
		//	for ( int iTex=0; iTex<pStudioHdr->numtextures; iTex++ )
		//	{
		//		V_strcat_safe( szConcatTextureNames, pStudioHdr->pTexture( iTex )->pszName() );
		//		//Msg( "    %s\n", pStudioHdr->pTexture( iTex )->pszName() );
		//	}
		//}
		//newPeer->m_szConcatTextureNames.Set( szConcatTextureNames );

		char szPhyPath[ MAX_PATH ];
		V_strcpy_safe( szPhyPath, newPeer->m_szMdlName.Get() );
		V_SetExtension( szPhyPath, ".phy", sizeof( szPhyPath ) );

		CUtlBuffer bufphy;
		if ( g_pFullFileSystem->ReadFile( szPhyPath, NULL, bufphy ) )
		{

			const phyfileheader_t *pHeader = reinterpret_cast<const phyfileheader_t *>( bufphy.Base() );
			if ( pHeader && pHeader->m_nHeaderSize == 16 && pHeader->m_nZero == 0 )
			{
				const solidheader_t *pSolid = pHeader->GetFirstSolid();

				CUtlVector<const convexleaf_t *> leafList;
				const treenode_t *pNode = pSolid->GetCollisionModel()->GetRoot();
				int nLeafCount = GetLeaves_r( leafList, pNode );

				newPeer->m_nHulls = nLeafCount;
				//Msg( "Found %i hulls in %s\n", newPeer->m_nHulls, szPhyPath );
			}
		}

		KeyValues *tempKeyValues = new KeyValues( "qc_path" );
		if ( tempKeyValues->LoadFromBuffer( NULL, pStudioHdr->KeyValueText() ) )
		{
			KeyValues *qc_path = tempKeyValues->FindKey( "qc_path", false );
			if ( qc_path )
			{

				const char* szQCPathRelative = qc_path->GetFirstValue()->GetString();

				char szTempAbsQCPath[ MAX_PATH ];
				if ( !g_pFullFileSystem->RelativePathToFullPath( szQCPathRelative, "CONTENT", szTempAbsQCPath, sizeof( szTempAbsQCPath ) ) )
				{
					Warning( "QC path read failure: %s\n", szQCPathRelative );
					return false;
				}

				CUtlBuffer bufQC( 0, 0, CUtlBuffer::TEXT_BUFFER );
				if ( g_pFullFileSystem->ReadFile( szTempAbsQCPath, NULL, bufQC ) )
				{

					CUtlVector<CUtlString> vecTokens;

					characterset_t breakSet;
					CharacterSetBuild( &breakSet, "{}()':" );

					char szToken[ 512 ];
					while ( bufQC.ParseToken( &breakSet, szToken, sizeof( szToken ), true ) != -1 )
					{
						if ( szToken[ 0 ] != '{' && szToken[ 0 ] != '}' ) // so much for break chars :(
							vecTokens[ vecTokens.AddToTail() ].Set( szToken );
					}

					int nTok = -1;

					nTok = vecTokens.Find( "$modelname" );
					if ( nTok != -1 && vecTokens.IsValidIndex( nTok + 1 ) )
					{
						char szMdlNameA[ 128 ];
						V_FileBase( szQCPathRelative, szMdlNameA, sizeof( szMdlNameA ) );

						const char* szModelTarget = vecTokens[ nTok + 1 ].Get();

						char szMdlNameB[ 128 ];
						V_FileBase( szModelTarget, szMdlNameB, sizeof( szMdlNameB ) );

						if ( V_strcmp( szMdlNameA, szMdlNameB ) )
						{
							Warning( "Model %s claims to come from %s, but that qc actually produces %s\n", newPeer->m_szMdlName.Get(), szQCPathRelative, szModelTarget );
						}
					}

					nTok = vecTokens.Find( "$scale" );
					if ( nTok != -1 && vecTokens.IsValidIndex( nTok + 1 ) )
					{
						const char* szScale = vecTokens[ nTok + 1 ].Get();

						newPeer->m_flScale = V_atof( szScale );

						if ( newPeer->m_flScale == 0 )
						{
							Warning( "Prop has zero scale! %s\n", szQCPathRelative );
							return false;
						}
						//else if ( newPeer->m_flScale != 1.0f )
						//{
						//	Msg( "Non-unit scale of %.2f on %s\n", newPeer->m_flScale, szQCPathRelative );
						//}
					}

					nTok = vecTokens.Find( "$body" );
					if ( nTok != -1 && vecTokens.IsValidIndex( nTok + 2 ) )
					{
						char szTokenClean[ MAX_PATH ];
						V_StripExtension( vecTokens[ nTok + 2 ].Get(), szTokenClean, sizeof( szTokenClean ) );

						char szQCFolder[ MAX_PATH ];
						V_ExtractFilePath( szQCPathRelative, szQCFolder, sizeof( szQCFolder ) );

						char szSrcPath[ MAX_PATH ];
						V_sprintf_safe( szSrcPath, "%s%s", szQCFolder, szTokenClean );

						newPeer->m_szRefName.Set( szSrcPath );

						nTok = vecTokens.Find( "$collisionmodel" );
						if ( nTok != -1 && vecTokens.IsValidIndex( nTok + 1 ) )
						{
							//V_strcpy_safe( szTokenClean, vecTokens[ nTok + 1 ].Get() );
							V_StripExtension( vecTokens[ nTok + 1 ].Get(), szTokenClean, sizeof( szTokenClean ) );

							V_sprintf_safe( szSrcPath, "%s%s", szQCFolder, szTokenClean );

							if ( vecTokens.Find( "$concave" ) != -1 )
							{
								newPeer->m_bConcave = true;
							}
						}

						newPeer->m_szPhyName.Set( szSrcPath );

						bExtractedSources = true;

					}

					nTok = vecTokens.Find( "$upaxis" );
					if ( nTok != -1 && vecTokens.IsValidIndex( nTok + 1 ) )
					{
						if ( V_stristr( vecTokens[ nTok + 1 ].Get(), "y" ) )
						{
							newPeer->m_bUpaxisY = true;
						}
					}

				}
			}
		}
	}

	if ( !bExtractedSources )
	{
		Warning( "Error loading studio model \"%s\"!\n", newPeer->m_szMdlName.Get() );
		return false;
	}

	char szTempPath[ MAX_PATH ];
	bool bExists = false;

	V_strcpy_safe( szTempPath, newPeer->m_szRefName.Get() );
	V_SetExtension( szTempPath, ".smd", sizeof( szTempPath ) );
	if ( g_pFullFileSystem->FileExists( szTempPath, "CONTENT" ) )
		bExists = true;

	V_SetExtension( szTempPath, ".fbx", sizeof( szTempPath ) );
	if ( g_pFullFileSystem->FileExists( szTempPath, "CONTENT" ) )
		bExists = true;

	V_SetExtension( szTempPath, ".dmx", sizeof( szTempPath ) );
	if ( g_pFullFileSystem->FileExists( szTempPath, "CONTENT" ) )
		bExists = true;

	if ( !bExists )
	{
		Warning( "Static prop combine src doesn't exist: %s\n", szTempPath );
		return false;
	}

	return true;
}

bool LoadSPCombineRules( void )
{
#ifndef STATIC_PROP_COMBINE_ENABLED
	return false;
#endif

#ifndef IS_WINDOWS_PC
	return false;
#endif

	if ( !staticpropcombine ) // feature must be enabled via commandline
		return false;

	if ( kvSPCombineRules->LoadFromFile( g_pFullFileSystem, "scripts/hammer/spcombinerules/spcombinerules.txt", "GAME" ) )
	{

		CUtlVector<CUtlString> vecUniqueModelNames;

		for ( KeyValues *kGroup = kvSPCombineRules->GetFirstSubKey(); kGroup != NULL; kGroup = kGroup->GetNextKey() )
		{
			//if ( !V_stristr( kGroup->GetName(), "keep" ) )
			//	continue;

			staticpropcombinerule_t *pNewGroup = &g_vecCombineRules[ g_vecCombineRules.AddToTail() ];

			pNewGroup->m_szGroupName.Set( kGroup->GetName() );
			pNewGroup->m_szQcTemplatePath.Set( kGroup->GetString( "qc_template_path", NULL ) );

			pNewGroup->m_nClusterLimit = kGroup->GetInt( "cluster_limit", DEFAULT_COMBINE_STATIC_PROP_COUNT );
			pNewGroup->m_flDistanceLimit = kGroup->GetInt( "distance_limit", DEFAULT_COMBINE_STATIC_PROP_DISTANCE );

			KeyValues *pPeers = kGroup->FindKey( "peers" );
			for ( KeyValues *pPeer = pPeers->GetFirstSubKey(); pPeer != NULL; pPeer = pPeer->GetNextKey() )
			{
				staticpropcombinepeer_t *newPeer = &pNewGroup->m_vecCombinePeers[ pNewGroup->m_vecCombinePeers.AddToTail() ];

				newPeer->m_szMdlName.Set( pPeer->GetName() );

				if ( vecUniqueModelNames.Count() && vecUniqueModelNames.Find( newPeer->m_szMdlName ) != -1 )
				{
					Error( "Model %s in group %s cannot be defined twice!\n", newPeer->m_szMdlName, pNewGroup->m_szGroupName.Get() );
				}
				else
				{
					vecUniqueModelNames.AddToTail( newPeer->m_szMdlName );
				}

				if ( !InitStaticPropCombinePeer( newPeer ) )
				{
					return false;
				}
			}

			if ( !pNewGroup->m_szQcTemplatePath || pNewGroup->m_vecCombinePeers.Count() == 0 )
			{
				Error( "Failed to parse static prop combine rule group: \"%s\"\n", pNewGroup->m_szGroupName.Get() );
			}
		}
		

		//FOR_EACH_VEC( g_vecCombineRules, i )
		//{
		//	staticpropcombinepeer_t *pPeer1 = &g_vecCombineRules[i].m_vecCombinePeers[0];
		//
		//	FOR_EACH_VEC( g_vecCombineRules[i].m_vecCombinePeers, j )
		//	{
		//		staticpropcombinepeer_t *pPeer2 = &g_vecCombineRules[i].m_vecCombinePeers[j];
		//
		//		if ( V_strcmp( pPeer1->m_szConcatTextureNames.Get(), pPeer2->m_szConcatTextureNames.Get() ) )
		//		{
		//			Msg( "Mismatch materials: %s to %s\n", pPeer1->m_szConcatTextureNames.Get(), pPeer2->m_szConcatTextureNames.Get() );
		//			Msg( "  %s\n", pPeer2->m_szMdlName.Get() );
		//		}
		//	
		//	}		
		//}

		return true;
	}
	else
	{
		delete kvSPCombineRules;
		kvSPCombineRules = NULL;
	}

	return false;
}

int GetCombineRuleForProp( const char *pszPropName1 )
{
	if ( !g_vecCombineRules.Count() )
		return -1;

	FOR_EACH_VEC( g_vecCombineRules, i )
	{
		FOR_EACH_VEC( g_vecCombineRules[i].m_vecCombinePeers, j )
		{
			staticpropcombinepeer_t *pPeer = &g_vecCombineRules[i].m_vecCombinePeers[j];
			
			if ( !V_strcmp( pszPropName1, pPeer->m_szMdlName.Access() ) )
				return i;
		}		
	}

	return -1;
}

bool StaticPropsMatchSkin( StaticPropBuild_t* prop1, StaticPropBuild_t* prop2 )
{
	int nSkin1 = prop1->m_Skin;
	int nSkin2 = prop2->m_Skin;

	return ( nSkin1 == nSkin2 );
}

bool StaticPropsMatchFlags( StaticPropBuild_t* prop1, StaticPropBuild_t* prop2 )
{
	#define HelperCompareFlags( _flag ) if ( staticpropcombine_doflagcompare_##_flag && ((prop1->m_Flags & _flag) != 0) != ((prop2->m_Flags & _flag) != 0) ) { return false; }

	HelperCompareFlags( STATIC_PROP_IGNORE_NORMALS )
	HelperCompareFlags( STATIC_PROP_NO_SHADOW )
	HelperCompareFlags( STATIC_PROP_NO_FLASHLIGHT )
	HelperCompareFlags( STATIC_PROP_MARKED_FOR_FAST_REFLECTION )
	HelperCompareFlags( STATIC_PROP_NO_PER_VERTEX_LIGHTING )
	HelperCompareFlags( STATIC_PROP_NO_SELF_SHADOWING )
	HelperCompareFlags( STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH )

	return true;
}

bool StaticPropsAreGroupPeers( StaticPropBuild_t* prop1, StaticPropBuild_t* prop2, int nCombineRule )
{
	if ( !g_vecCombineRules.Count() || nCombineRule >= g_vecCombineRules.Count() )
		return false;
	
	bool bProp1Present = false;
	bool bProp2Present = false;

	FOR_EACH_VEC( g_vecCombineRules[nCombineRule].m_vecCombinePeers, j )
	{
		staticpropcombinepeer_t *pPeer = &g_vecCombineRules[nCombineRule].m_vecCombinePeers[j];
		
		if ( !bProp1Present && !V_strcmp( prop1->m_pModelName, pPeer->m_szMdlName.Access() ) )
		{
			bProp1Present = true;

			if ( !prop1->m_bCombineDataWritten )
			{
				prop1->m_bCombineDataWritten = true;

				prop1->m_szRefName = pPeer->m_szRefName;
				prop1->m_szPhyName = pPeer->m_szPhyName;
				prop1->m_nHulls = pPeer->m_nHulls;
				prop1->m_bConcave = pPeer->m_bConcave;
				prop1->m_bUpaxisY = pPeer->m_bUpaxisY;
				prop1->m_flScale = pPeer->m_flScale;
				prop1->m_nCombineRuleGroup = nCombineRule;
			}
		}

		if ( !bProp2Present && !V_strcmp( prop2->m_pModelName, pPeer->m_szMdlName.Access() ) )
		{
			bProp2Present = true;

			if ( !prop2->m_bCombineDataWritten )
			{
				prop2->m_bCombineDataWritten = true;

				prop2->m_szRefName = pPeer->m_szRefName;
				prop2->m_szPhyName = pPeer->m_szPhyName;
				prop2->m_nHulls = pPeer->m_nHulls;
				prop2->m_bConcave = pPeer->m_bConcave;
				prop2->m_bUpaxisY = pPeer->m_bUpaxisY;
				prop2->m_flScale = pPeer->m_flScale;
				prop2->m_nCombineRuleGroup = nCombineRule;
			}
		}

		if ( bProp1Present && bProp2Present )
			return true;
	}

	return false;
}

bool CompileQC( const char *pFileName )
{
	// Spawn studiomdl.exe process to generate .mdl and associated files
	char cmdline[ 2 * MAX_PATH + 256 ];
	V_snprintf( cmdline, sizeof( cmdline ), "studiomdl.exe -nop4 -quiet %s", pFileName );

#ifdef IS_WINDOWS_PC
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	ZeroMemory( &pi, sizeof(pi) );

	// Start the child process. 
	if ( CreateProcess( NULL,	// No module name (use command line). 
		cmdline,	// Command line. 
		NULL,				// Process handle not inheritable. 
		NULL,				// Thread handle not inheritable. 
		FALSE,				// Set handle inheritance to FALSE. 
		0,					// No creation flags. 
		NULL,				// Use parent's environment block. 
		NULL,				// Use parent's starting directory. 
		&si,				// Pointer to STARTUPINFO structure.
		&pi ) )				// Pointer to PROCESS_INFORMATION structure.
	{

		// Successfully created the process.  Wait for it to finish.
		WaitForSingleObject( pi.hProcess, INFINITE );
		
		// Get the exit code.
		DWORD exitCode;
		BOOL result = GetExitCodeProcess( pi.hProcess, &exitCode );
		
		// Close the handles.
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );
		
		if (!result)
		{
			return false;
		}
		
		return true;
	}
#endif

	return false;
}

void CombineStaticProps( propBuildVector &vecGroup, int &nCombineIndex )
{
	if ( vecGroup.Count() == 0 )
		Error( "Combine group has zero members!\n" );
	
	if ( !vecGroup[0].m_bCombineDataWritten )
		Error( "Don't know how to combine model: %s!\n", vecGroup[0].m_pModelName );

	// record into stats
	g_vecCombineRules[ vecGroup[0].m_nCombineRuleGroup ].m_vecStatsMemberCounts.AddToTail( vecGroup.Count() );

	const char* szCombineRuleGroupName = g_vecCombineRules[ vecGroup[0].m_nCombineRuleGroup ].m_szGroupName.Get();

	// let's build the temp qc
	bool bLoadQcTemplate = true;
	const char* szQCPath = g_vecCombineRules[ vecGroup[ 0 ].m_nCombineRuleGroup ].m_szQcTemplatePath.Get();
	if ( strlen( szQCPath ) <= 0 )
	{
		bLoadQcTemplate = false;
		if ( g_vecCombineRules[ vecGroup[ 0 ].m_nCombineRuleGroup ].m_bufAutoGeneratedQCTemplate.Size() == 0 )
		{
			Error( "Invalid qc template path for group: %s!\n", szCombineRuleGroupName );
		}
	}

	CUtlBuffer bufQCTemplate( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( bLoadQcTemplate )
	{
		if ( !g_pFullFileSystem->ReadFile( szQCPath, NULL, bufQCTemplate ) )
		{
			Error( "Couldn't load the temp qc from: %s!\n", szQCPath );
		}
	}
	else
	{
		bufQCTemplate.PutString( (const char*)g_vecCombineRules[ vecGroup[ 0 ].m_nCombineRuleGroup ].m_bufAutoGeneratedQCTemplate.Base() );
	}

	nCombineIndex++;

	CUtlBuffer bufQC( 0, 0, CUtlBuffer::TEXT_BUFFER );

	char szModelName[ 512 ];
	V_sprintf_safe( szModelName, "props/autocombine/%s/_autocombine_%s_%i.mdl", mapbase, szCombineRuleGroupName, nCombineIndex );

#ifdef DEBUG
	Msg( "Building cluster model: %i members of \"%s\" into \"_autocombine_%s_%i.mdl\"\n", vecGroup.Count(), szCombineRuleGroupName, szCombineRuleGroupName, nCombineIndex );
#else
	Msg( "." );
#endif

	bufQC.Printf( "$modelname %s\n\n$contentrootrelative\n\n", szModelName );

	if ( vecGroup[ 0 ].m_bUpaxisY )
	{
		// if the source basemodel is upaxis y, we need to build the combine model base in the same coordsys
		bufQC.Printf( "$upaxis y\n\n" );
	}

	float flRootScale = vecGroup[ 0 ].m_flScale;
	if ( flRootScale != 1.0f )
	{
		if ( flRootScale == 0 )
			Error( "Zero scale error!\n" );

		bufQC.Printf( "$scale %.3f\n\n", flRootScale );
	}

	bufQC.Printf( "$body body \"%s\"\n\n", vecGroup[ 0 ].m_szRefName.Get() );

	Vector vecRootPos = vecGroup[ 0 ].m_Origin;
	QAngle angRootAng = vecGroup[ 0 ].m_Angles;

	matrix3x4_t matRoot;
	AngleMatrix( angRootAng, vecRootPos, matRoot );

	CUtlVector<matrix3x4_t> vecOffsets;
	vecOffsets.EnsureCapacity( vecGroup.Count() );
	FOR_EACH_VEC( vecGroup, v )
	{
		vecOffsets[ vecOffsets.AddToTail() ].SetToIdentity();
	}

	CUtlVector<matrix3x4_t> vecOffsetsPhy;
	vecOffsetsPhy.EnsureCapacity( vecGroup.Count() );
	FOR_EACH_VEC( vecGroup, v )
	{
		vecOffsetsPhy[ vecOffsetsPhy.AddToTail() ].SetToIdentity();
	}

	// add in the extra props to combine at their relative offset to the root prop
	FOR_EACH_VEC( vecGroup, i )
	{
		if ( i == 0 )
			continue;

		if ( !vecGroup[ i ].m_bCombineDataWritten )
			Error( "Don't know how to combine model: %s!\n", vecGroup[ i ].m_pModelName );

		bufQC.Printf( "$appendsource \"%s\" ", vecGroup[ i ].m_szRefName.Get() );

		Vector vecAddonPos = vecGroup[ i ].m_Origin;
		QAngle angAddonAng = vecGroup[ i ].m_Angles;

		// get xform relative to root (in hammer space)
		matrix3x4_t matLocal;
		AngleMatrix( angAddonAng, vecAddonPos, matLocal );
		ConcatTransforms( matRoot.InverseTR(), matLocal, matLocal );

		// see: https://intranet.valvesoftware.com/wiki/3D_Coordinate_Systems
		matrix3x4_t matRotate; matRotate.InitFromQAngles( QAngle( 0, -90, 0 ) );
		ConcatTransforms( matRotate, matLocal, matLocal );
		matrix3x4_t matConvert;	matConvert.InitXYZ( Vector( 0, -1, 0 ), Vector( 1, 0, 0 ), Vector( 0, 0, 1 ), Vector( 0, 0, 0 ) );
		ConcatTransforms( matLocal, matConvert.InverseTR(), matLocal );

		MatrixCopy( matLocal, vecOffsetsPhy[ i ] );

		if ( vecGroup[ i ].m_bUpaxisY )
		{
			// child combine models with upaxis y need to get flipped around too
			matRotate.InitFromQAngles( QAngle( 0, 0, -90 ) );
			ConcatTransforms( matRotate, matLocal, matLocal );
			matConvert.InitXYZ( Vector( 1, 0, 0 ), Vector( 0, 0, -1 ), Vector( 0, 1, 0 ), Vector( 0, 0, 0 ) );
			ConcatTransforms( matLocal, matConvert.InverseTR(), matLocal );
		}

		MatrixCopy( matLocal, vecOffsets[ i ] );

		// export position + qangles
		MatrixAngles( vecOffsets[ i ], angAddonAng, vecAddonPos );

		bufQC.Printf( "\"offset pos[" );

		bufQC.Printf( " %.3f", vecAddonPos.x );
		bufQC.Printf( " %.3f", vecAddonPos.y );
		bufQC.Printf( " %.3f", vecAddonPos.z );

		bufQC.Printf( " ] angle[" );

		bufQC.Printf( " %.3f", angAddonAng.x );
		bufQC.Printf( " %.3f", angAddonAng.y );
		bufQC.Printf( " %.3f", angAddonAng.z );

		bufQC.Printf( " ] scale[" );

		bufQC.Printf( " %.3f ]\"\n", vecGroup[ i ].m_flScale );

	}

	bufQC.Printf( CUtlString( bufQCTemplate.Base(), bufQCTemplate.Size() ) );

	bufQC.Printf( "\n$sequence \"idle\" \"%s\" act_idle 1\n\n", vecGroup[ 0 ].m_szRefName.Get() );


	// add the physics hulls
	bool bDoPhysics = false;
	FOR_EACH_VEC( vecGroup, i )
	{
		if ( vecGroup[ i ].m_Solid != 0 )
		{
			bDoPhysics = true;
			break;
		}
	}

	if ( bDoPhysics )
	{
		bufQC.Printf( "$collisionprecision 0.01\n" );

		bufQC.Printf( "$collisionmodel \"blank\" {\n" );

		bufQC.Printf( "\t$maxconvexpieces 64\n" ); // should never even approach this limit due to smaller hull limit
		bufQC.Printf( "\t$automass\n" );
		bufQC.Printf( "\t$remove2d\n" );
		bufQC.Printf( "\t$concave\n" );

		FOR_EACH_VEC( vecGroup, i )
		{
			if ( !vecGroup[ i ].m_Solid )
				continue;

			bufQC.Printf( "\t$addconvexsrc \"%s\" ", vecGroup[ i ].m_szPhyName.Get() );

			// export position + qangles
			Vector vecAddonPos;
			QAngle angAddonAng;
			MatrixAngles( vecOffsetsPhy[ i ], angAddonAng, vecAddonPos );

			bufQC.Printf( "\"offset pos[" );

			bufQC.Printf( " %.3f", vecAddonPos.x );
			bufQC.Printf( " %.3f", vecAddonPos.y );
			bufQC.Printf( " %.3f", vecAddonPos.z );

			bufQC.Printf( " ] angle[" );

			bufQC.Printf( " %.3f", angAddonAng.x );
			bufQC.Printf( " %.3f", angAddonAng.y );
			bufQC.Printf( " %.3f", angAddonAng.z );

			bufQC.Printf( " ] scale[" );

			bufQC.Printf( " %.3f ]\"", vecGroup[ i ].m_flScale );

			if ( vecGroup[ i ].m_bConcave )
			{
				bufQC.Printf( " concave " );
			}

			bufQC.Printf( "\n" );

		}

		bufQC.Printf( "}\n" );

		// important! if any members have physics hulls, the root model needs to be flagged as such.
		vecGroup[ 0 ].m_Solid = 6; // 6 is "Use VPhysics"

	}


	char szMapSpecificAutocombineQcDir[ MAX_PATH ];
	V_sprintf_safe( szMapSpecificAutocombineQcDir, "models/props/autocombine/%s/", mapbase );
	g_pFullFileSystem->CreateDirHierarchy( szMapSpecificAutocombineQcDir, "CONTENT" );

	char szGeneratedQCpath[ MAX_PATH ];
	V_sprintf_safe( szGeneratedQCpath, "models/props/autocombine/%s/autocombine_%s_%i.qc", mapbase, szCombineRuleGroupName, nCombineIndex );

	if ( g_pFullFileSystem->WriteFile( szGeneratedQCpath, "CONTENT", bufQC ) )
	{
		char szTempAbsPath[ MAX_PATH ];
		g_pFullFileSystem->RelativePathToFullPath( szGeneratedQCpath, "CONTENT", szTempAbsPath, sizeof( szTempAbsPath ) );

		if ( CompileQC( szTempAbsPath ) )
		{
			char szModelNameModelPrefix[ 512 ];
			V_sprintf_safe( szModelNameModelPrefix, "models/%s", szModelName );
			int nStrIdx = g_vecGeneratedModelNames.AddToTail( CUtlString( szModelNameModelPrefix ) );

			// now bspzip the autogenerated models into the bsp

			char szTempAbsMdlPath[ MAX_PATH ];
			g_pFullFileSystem->RelativePathToFullPath( szModelNameModelPrefix, "GAME", szTempAbsMdlPath, sizeof( szTempAbsMdlPath ) );

			bool bBSPZipSuccess = false;

			// find the .mdl
			if ( g_pFullFileSystem->FileExists( szTempAbsMdlPath ) )
			{
				//add it
				AddFileToPak( GetPakFile(), szModelNameModelPrefix, szTempAbsMdlPath );
				if ( staticpropcombine_delsources )
					g_pFullFileSystem->RemoveFile( szModelNameModelPrefix, "GAME" );

				// find the .phy
				V_SetExtension( szTempAbsMdlPath, ".phy", sizeof( szTempAbsMdlPath ) );
				V_SetExtension( szModelNameModelPrefix, ".phy", sizeof( szModelNameModelPrefix ) );
				if ( g_pFullFileSystem->FileExists( szTempAbsMdlPath ) )
				{
					// add it
					AddFileToPak( GetPakFile(), szModelNameModelPrefix, szTempAbsMdlPath );
					if ( staticpropcombine_delsources )
						g_pFullFileSystem->RemoveFile( szModelNameModelPrefix, "GAME" );
				}
				else
				{
					// not a failure if it doesn't exist
					//Msg( "Couldn't find phy model: %s\n", szModelNameModelPrefix );
				}

				// find the .vvd
				V_SetExtension( szTempAbsMdlPath, ".vvd", sizeof( szTempAbsMdlPath ) );
				V_SetExtension( szModelNameModelPrefix, ".vvd", sizeof( szModelNameModelPrefix ) );
				if ( g_pFullFileSystem->FileExists( szTempAbsMdlPath ) )
				{
					// add it
					AddFileToPak( GetPakFile(), szModelNameModelPrefix, szTempAbsMdlPath );
					if ( staticpropcombine_delsources )
						g_pFullFileSystem->RemoveFile( szModelNameModelPrefix, "GAME" );

					// find the .dx90.vtx
					V_SetExtension( szTempAbsMdlPath, ".dx90.vtx", sizeof( szTempAbsMdlPath ) );
					V_SetExtension( szModelNameModelPrefix, ".dx90.vtx", sizeof( szModelNameModelPrefix ) );
					if ( g_pFullFileSystem->FileExists( szTempAbsMdlPath ) )
					{
						// add it
						AddFileToPak( GetPakFile(), szModelNameModelPrefix, szTempAbsMdlPath );
						if ( staticpropcombine_delsources )
							g_pFullFileSystem->RemoveFile( szModelNameModelPrefix, "GAME" );

						{
							// remove the qc
							if ( staticpropcombine_delsources )
								g_pFullFileSystem->RemoveFile( szTempAbsPath, "CONTENT" );

							vecGroup[ 0 ].m_pModelName = g_vecGeneratedModelNames[ nStrIdx ].Access();

							bBSPZipSuccess = true;
						}
					}
				}
			}

			if ( !bBSPZipSuccess )
			{
				Error( "Failed to bspzip autogenerated model into the map: %s!\n", szModelNameModelPrefix );
			}

			return;
		}
		else
		{
			Error( "Failed while compiling: %s!\n", szGeneratedQCpath );
		}
	}
	else
	{
		Error( "Couldn't write a temp qc to: %s!\n", szGeneratedQCpath );
	}
}

Vector vecTempSortOrigin;
int PropDistanceSortFunctionFarToNear( const StaticPropBuild_t* entry1, const StaticPropBuild_t* entry2 )
{
	if ( entry1->m_Origin.DistToSqr( vecTempSortOrigin ) > entry2->m_Origin.DistToSqr( vecTempSortOrigin ) )
	{
		return -1;
	}
	else
	{
		return 1;
	}

	// eh, don't care about the equal distance case
}

bool PropDiffuseModulationsMatch( const StaticPropBuild_t* entry1, const StaticPropBuild_t* entry2 )
{
	if ( entry1->m_DiffuseModulation.r == entry2->m_DiffuseModulation.r &&
		 entry1->m_DiffuseModulation.g == entry2->m_DiffuseModulation.g &&
		 entry1->m_DiffuseModulation.b == entry2->m_DiffuseModulation.b )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// figure out which leaf a point is in
//-----------------------------------------------------------------------------
static int PointLeafnum( const Vector& p, int num = 0 )
{
	float		d;
	while ( num >= 0 )
	{
		dnode_t* node = dnodes + num;
		dplane_t* plane = dplanes + node->planenum;

		if ( plane->type < 3 )
			d = p[ plane->type ] - plane->dist;
		else
			d = DotProduct( plane->normal, p ) - plane->dist;
		if ( d < 0 )
			num = node->children[ 1 ];
		else
			num = node->children[ 0 ];
	}

	return -1 - num;
}

bool IsCompletelyInSolid( const StaticPropBuild_t* entry1 )
{
	// Get the collision model
	CPhysCollide* pConvexHull = GetCollisionModel( entry1->m_pModelName );
	if ( !pConvexHull )
		return false;

	// Compute the leaves the static prop's convex hull hits
	CUtlVector< unsigned short > leafList;
	ComputeStaticPropLeaves( pConvexHull, entry1->m_Origin, entry1->m_Angles, leafList );
	for ( int i = 0; i < leafList.Count(); i++ )
	{
		int cluster = dleafs[ leafList[ i ] ].cluster;
		if ( cluster != -1 )
		{
			return false;
		}
	}
	return true;
}

bool StaticPropLocationsMatch( const StaticPropBuild_t* entry1, const StaticPropBuild_t* entry2, int nCombineRule )
{
	if ( staticpropcombine_considervis )
	{
		// Get the collision model
		CPhysCollide* pConvexHull = GetCollisionModel( entry1->m_pModelName );
		if ( !pConvexHull )
			return false;

		// Compute the leaves the static prop's convex hull hits
		CUtlVector< unsigned short > leafList;
		ComputeStaticPropLeaves( pConvexHull, entry1->m_Origin, entry1->m_Angles, leafList );
		CUtlVector<int> clusterList1;
		for ( int i = 0; i < leafList.Count(); i++ )
		{
			int cluster = dleafs[ leafList[ i ] ].cluster;
			if ( cluster != -1 )
			{
				clusterList1.FindAndFastRemove( cluster );
				clusterList1.AddToTail( cluster );
			}
		}

		pConvexHull = GetCollisionModel( entry2->m_pModelName );
		if ( !pConvexHull )
			return false;

		// Compute the leaves the static prop's convex hull hits
		leafList.RemoveAll();
		ComputeStaticPropLeaves( pConvexHull, entry2->m_Origin, entry2->m_Angles, leafList );
		CUtlVector<int> clusterList2;
		for ( int i = 0; i < leafList.Count(); i++ )
		{
			int cluster = dleafs[ leafList[ i ] ].cluster;
			if ( cluster != -1 )
			{
				clusterList2.FindAndFastRemove( cluster );
				clusterList2.AddToTail( cluster );
			}
		}
		clusterList1.Sort();
		clusterList2.Sort();
		for ( int i = 0; i < clusterList1.Count(); i++ )
		{
			if ( clusterList2.Find( clusterList1[ i ] ) >= 0 )
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return entry1->m_Origin.DistTo( entry2->m_Origin ) < g_vecCombineRules[ nCombineRule ].m_flDistanceLimit; // candidate must be close enough to the cluster root
	}
}

bool GroupProp( propBuildVector &vecSearch, propBuildVector &vecMoveTo )
{
	if ( !vecSearch.Count() )
	{
		//Msg( "Closing group. There are no more props to group!\n" );
		return false;
	}

	if ( !vecMoveTo.Count() )
	{
		//Msg( "\nStarting NEW cluster with: %s\n", vecSearch[0].m_pModelName );

		vecMoveTo.AddToTail( vecSearch[0] );
		vecSearch.Remove(0);
		return true;
	}

	int nCombineRule = GetCombineRuleForProp( vecMoveTo[0].m_pModelName );
	if ( nCombineRule < 0 )
	{
		//Msg( "Closing group. (No combine rules include %s)\n", vecMoveTo[0].m_pModelName );
		return false;
	}

	if ( vecMoveTo.Count() > g_vecCombineRules[nCombineRule].m_nClusterLimit )
	{
		//Msg( "Closing group. (Combine limit reached: %i)\n", vecMoveTo.Count() );
		return false;
	}

	// there's a limit of MAX_HULLS to prevent creating super complex concave hulls
	int nHulls = 0;
	int nParts = 0;
	FOR_EACH_VEC( vecMoveTo, i )
	{
		if ( vecMoveTo[i].m_Solid != 0 )
		{
			nParts++;
			nHulls += vecMoveTo[i].m_nHulls;
		}
	}
	if ( nHulls >= MAX_HULLS || nParts >= MAX_EXTRA_COLLISION_MODELS )
	{
		//Msg( "Closing group. (Convex hull limit reached: %i)\n", nHulls );
		return false;
	}

	// sort vecSearch from far to near, so the closest entries are at the end of the list.
	vecTempSortOrigin = vecMoveTo[0].m_Origin;
	vecSearch.Sort( PropDistanceSortFunctionFarToNear );

	// look through the as-yet ungrouped props and find the best one to be the next part of the group we're currently building
	FOR_EACH_VEC_BACK( vecSearch, i )
	{
		if ( PropDiffuseModulationsMatch( &vecSearch[i], &vecMoveTo[0] ) && // candidate must have the same diffuse modulation
			StaticPropLocationsMatch(&vecSearch[i], &vecMoveTo[0], nCombineRule ) &&	 // candidate must be close enough to the cluster root
			StaticPropsAreGroupPeers( &vecSearch[i], &vecMoveTo[0], nCombineRule ) && // must be allowed to combine by script rules
			StaticPropsMatchSkin( &vecSearch[i], &vecMoveTo[0] ) && // must have the same skin group
			StaticPropsMatchFlags( &vecSearch[i], &vecMoveTo[0] ) ) // must have matching flags
		{

			//Msg( "   Add to cluster: %s\n", vecSearch[i].m_pModelName );

			vecMoveTo.AddToTail( vecSearch[i] );
			vecSearch.Remove(i);
			return true;
		}
	}

	//Msg( "Closing group. (No peers found for %s)\n", vecMoveTo[0].m_pModelName );

	return false;
}

inline bool UtlStringLessFunc( const CUtlString &lhs, const CUtlString &rhs )
{
	return V_strcmp( lhs.Get(), rhs.Get() ) < 0;
}

struct MaterialUsageInfo_t
{
	CUtlString matName;
	int nMaterialCount;	// I make a fake MaterialUsageInfo with all material names concatenated for models with multiple materials
	int nInstanceCount;
	CUtlString surfaceProp;
	CCopyableUtlVector<CUtlString> modelsUsingThisMaterial;
};

void CreatePropCombineRule( const MaterialUsageInfo_t &materialInstances )
{
	staticpropcombinerule_t rule;
	rule.m_szGroupName = materialInstances.matName;
	rule.m_szGroupName = rule.m_szGroupName.GetBaseFilename();
	rule.m_flDistanceLimit = 500.0f;
	rule.m_nClusterLimit = 32;			//FIXME: Determine based on # of verts in meshes?
	for ( int i = 0; i < materialInstances.modelsUsingThisMaterial.Count(); i++ )
	{
		staticpropcombinepeer_t peer;
		peer.m_szMdlName = materialInstances.modelsUsingThisMaterial[ i ];
		if ( !InitStaticPropCombinePeer( &peer ) )
		{
			return;
		}
		rule.m_vecCombinePeers.AddToTail( peer );
	}

	CUtlString materialPath = materialInstances.matName;
	materialPath = materialPath.DirName();
	rule.m_bufAutoGeneratedQCTemplate.Printf(
		"$cdmaterials %s\n"
		"\n"
		"$staticprop\n"
		"$surfaceprop \"%s\"\n", materialPath.Get(), rule.m_vecCombinePeers[ 0 ].m_surfaceProp.Get() );
	g_vecCombineRules.AddToTail( rule );
}


void AutoCreatePropCombineRules( propBuildVector& vecBuilds )
{
	CUtlMap<CUtlString, int, int> uniqueModels( UtlStringLessFunc );

	struct MaterialInfo_t
	{
		int nMaterialCount;
		int nStaticPropInstancesUsingMaterial;
		CCopyableUtlVector<CUtlString> modelsUsingThisMaterial;
	};
	CUtlMap<CUtlString, MaterialInfo_t, int> uniqueMaterials( UtlStringLessFunc );

	// Count unique props and count # of instances
	for ( int i = 0; i < vecBuilds.Count(); i++ )
	{
		const StaticPropBuild_t &build = vecBuilds[ i ];
		int nIdx = uniqueModels.Find( build.m_pModelName );
		if ( uniqueModels.IsValidIndex( nIdx ) )
		{
			uniqueModels[ nIdx ]++;
		}
		else
		{
			uniqueModels.Insert( build.m_pModelName, 1 );
		}
	}

	struct ModelInstanceInfo_t
	{
		CUtlString modelName;
		int nInstanceCount;
	};
	CUtlVector<ModelInstanceInfo_t> sortedModels;

	for ( int i = uniqueModels.FirstInorder(); uniqueModels.IsValidIndex( i ); i = uniqueModels.NextInorder( i ) )
	{
		ModelInstanceInfo_t e;
		e.modelName = uniqueModels.Key( i );
		e.nInstanceCount = uniqueModels[ i ];
		sortedModels.AddToTail( e );
	}
	std::sort( sortedModels.begin(), sortedModels.end(), []( const ModelInstanceInfo_t& a, const ModelInstanceInfo_t& b ){ return a.nInstanceCount > b.nInstanceCount; } );
	Msg( "%d unique static props used in map\n", sortedModels.Count() );
	

	for ( const ModelInstanceInfo_t &e : sortedModels )
	{
		CUtlBuffer buf;
		if ( !LoadStudioModel( e.modelName.Get(), "prop_static", buf ) )
		{
			Error( "Error loading studio model \"%s\"!\n", e.modelName.Get() );
			continue;
		}
		studiohdr_t* pStudioHdr = (studiohdr_t*)buf.PeekGet();
		if ( !pStudioHdr )
		{
			Error( "Error loading studio model \"%s\"!\n", e.modelName.Get() );
			continue;
		}

		CUtlString combinedMatName;
		for ( int i = 0; i < pStudioHdr->numtextures; i++ )
		{
			combinedMatName += pStudioHdr->pTexture( i )->pszName();
			if ( i < pStudioHdr->numtextures - 1 )
			{
				combinedMatName += "___";
			}
		}

		int nIdx = uniqueMaterials.Find( combinedMatName );
		if ( uniqueMaterials.IsValidIndex( nIdx ) )
		{
			uniqueMaterials[ nIdx ].nStaticPropInstancesUsingMaterial += e.nInstanceCount;
			uniqueMaterials[ nIdx ].modelsUsingThisMaterial.AddToTail( e.modelName );
		}
		else
		{
			MaterialInfo_t mi;
			mi.nStaticPropInstancesUsingMaterial = e.nInstanceCount;
			mi.nMaterialCount = pStudioHdr->numtextures;
			mi.modelsUsingThisMaterial.AddToTail( e.modelName );
			uniqueMaterials.Insert( combinedMatName, mi );
		}
	}

	CUtlVector<MaterialUsageInfo_t> sortedMaterials;
	for ( int i = uniqueMaterials.FirstInorder(); uniqueMaterials.IsValidIndex( i ); i = uniqueMaterials.NextInorder( i ) )
	{
		MaterialUsageInfo_t e;
		e.matName = uniqueMaterials.Key( i );
		e.nMaterialCount = uniqueMaterials[ i ].nMaterialCount;
		e.nInstanceCount = uniqueMaterials[ i ].nStaticPropInstancesUsingMaterial;
		e.modelsUsingThisMaterial.AddVectorToTail( uniqueMaterials[ i ].modelsUsingThisMaterial );
		sortedMaterials.AddToTail( e );
	}

	int nCreatedRuleCount = 0;

	std::sort( sortedMaterials.begin(), sortedMaterials.end(), []( const MaterialUsageInfo_t& a, const MaterialUsageInfo_t& b ){ return a.nInstanceCount > b.nInstanceCount; } );

	if ( staticpropcombine_autocombine )
	{
		int nInstancesCovered = 0;
		// Create combine rules for highly-instanced models with a single material
		for ( const MaterialUsageInfo_t &e : sortedMaterials )
		{
			if ( e.nMaterialCount > 1 )
			{
				continue;
			}
			if ( e.nInstanceCount >= 30 )
			{
				CreatePropCombineRule( e );
				nCreatedRuleCount++;
				nInstancesCovered += e.nInstanceCount;
			}
		}
		Msg( "Auto-created %d static prop combine rules (covering %d static prop instances) based on static prop and material usage.\n", nCreatedRuleCount, nInstancesCovered );
	}

	if ( staticpropcombine_suggestcombinerules )
	{
		// Print suggestions for what combine rules to create manually for models that use multiple materials
		Msg( "\nSuggested models for manually creating rules in spcombinerules.txt:\n" );
		for ( const MaterialUsageInfo_t &e : sortedMaterials )
		{
			if ( e.nMaterialCount == 1 || e.nInstanceCount < 30 )
			{
				continue;
			}
			Msg( "%4d %s\n", e.nInstanceCount, e.matName.Get() );
			for ( const CUtlString &modelName : e.modelsUsingThisMaterial )
			{
				Msg( "\t%s\n", modelName.Get() );
			}
		}
	}
}

void EmitStaticProps()
{
	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	if ( physicsFactory )
	{
		s_pPhysCollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
		if( !s_pPhysCollision )
			return;
	}

	// Generate a list of lighting origins, and strip them out
	int i;
	for ( i = 0; i < num_entities; ++i)
	{
		char* pEntity = ValueForKey(&entities[i], "classname");
		if (!Q_strcmp(pEntity, "info_lighting"))
		{
			s_LightingInfo.AddToTail(i);
		}
	}

	propBuildVector vecBuilds;
	bool bDoStaticPropCombine = LoadSPCombineRules();

	// Emit specifically specified static props
	for ( i = 0; i < num_entities; ++i)
	{
		char* pEntity = ValueForKey(&entities[i], "classname");
		if (!strcmp(pEntity, "static_prop") || !strcmp(pEntity, "prop_static"))
		{
			StaticPropBuild_t build;

			build.m_bCombineDataWritten = false;
			build.m_szRefName = NULL;
			build.m_szPhyName = NULL;
			build.m_nHulls = 0;
			build.m_bConcave = false;
			build.m_bUpaxisY = false;
			build.m_flScale = 1.0f;
			build.m_nCombineRuleGroup = -1;

			GetVectorForKey( &entities[i], "origin", build.m_Origin );
			GetAnglesForKey( &entities[i], "angles", build.m_Angles );
			build.m_pModelName = ValueForKey( &entities[i], "model" );
			build.m_Solid = IntForKey( &entities[i], "solid" );
			build.m_Skin = IntForKey( &entities[i], "skin" );
			build.m_FadeMaxDist = FloatForKey( &entities[i], "fademaxdist" );
			build.m_Flags = 0;//IntForKey( &entities[i], "spawnflags" ) & STATIC_PROP_WC_MASK;
			build.m_FlagsEx = 0;//IntForKey( &entities[i], "spawnflags" ) & STATIC_PROP_WC_MASK;
			if (IntForKey( &entities[i], "ignorenormals" ) == 1)
			{
				build.m_Flags |= STATIC_PROP_IGNORE_NORMALS;
			}
			if (IntForKey( &entities[i], "disableshadows" ) == 1)
			{
				build.m_Flags |= STATIC_PROP_NO_SHADOW;
			}
			if (IntForKey( &entities[i], "disableflashlight" ) == 1)
			{
				build.m_Flags |= STATIC_PROP_NO_FLASHLIGHT;
			}
			if (IntForKey( &entities[i], "drawinfastreflection" ) == 1)
			{
				build.m_Flags |= STATIC_PROP_MARKED_FOR_FAST_REFLECTION;
			}
			if (IntForKey( &entities[i], "disablevertexlighting" ) == 1)
			{
				build.m_Flags |= STATIC_PROP_NO_PER_VERTEX_LIGHTING;
			}
			if (IntForKey( &entities[i], "disableselfshadowing" ) == 1)
			{
				build.m_Flags |= STATIC_PROP_NO_SELF_SHADOWING;
			}
			if (IntForKey( &entities[i], "disableshadowdepth" ) == 1)
			{
				build.m_FlagsEx |= STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH;
			}
			if ( IntForKey( &entities[ i ], "enablelightbounce" ) == 1 )
			{
				build.m_FlagsEx |= STATIC_PROP_FLAGS_EX_ENABLE_LIGHT_BOUNCE;
			}

			if (IntForKey( &entities[i], "screenspacefade" ) == 1)
			{
				Warning( "Encountered obsolete static prop option to do its fade in screen space @ %.2f %.2f %.2f\n",
					build.m_Origin.x, build.m_Origin.y, build.m_Origin.z );
			}

			const char *pKey = ValueForKey( &entities[i], "fadescale" );
			if ( pKey && pKey[0] )
			{
				build.m_flForcedFadeScale = FloatForKey( &entities[i], "fadescale" );
			}
			else
			{
				build.m_flForcedFadeScale = 1;
			}
			build.m_FadesOut = (build.m_FadeMaxDist > 0);
			build.m_pLightingOrigin = ValueForKey( &entities[i], "lightingorigin" );
			if (build.m_FadesOut)
			{			  
				build.m_FadeMinDist = FloatForKey( &entities[i], "fademindist" );
				if (build.m_FadeMinDist < 0)
				{
					build.m_FadeMinDist = build.m_FadeMaxDist; 
				}
			}
			else
			{
				build.m_FadeMinDist = 0;
			}
			build.m_nMinCPULevel = (unsigned char)IntForKey( &entities[i], "mincpulevel" );
			build.m_nMaxCPULevel = (unsigned char)IntForKey( &entities[i], "maxcpulevel" );
			build.m_nMinGPULevel = (unsigned char)IntForKey( &entities[i], "mingpulevel" );
			build.m_nMaxGPULevel = (unsigned char)IntForKey( &entities[i], "maxgpulevel" );
			if ( build.m_nMaxCPULevel && build.m_nMaxCPULevel < build.m_nMinCPULevel )
			{
				build.m_nMaxCPULevel = build.m_nMinCPULevel;
			}
			if ( build.m_nMaxGPULevel && build.m_nMaxGPULevel < build.m_nMinGPULevel )
			{
				build.m_nMaxGPULevel = build.m_nMinGPULevel;
			}

			// FIXME: look for ComputeFXBlend and make sure that you don't
			// need a particlar rendermode for this stuff to happen
			// Get the per-instance render-color for this static prop
			const char *pColorKey = ValueForKey( &entities[i], "rendercolor" );
			if ( *pColorKey != '\0' )
			{
				color32 tmp;
				V_StringToColor32( &tmp, pColorKey );
				build.m_DiffuseModulation.r = tmp.r;
				build.m_DiffuseModulation.g = tmp.g;
				build.m_DiffuseModulation.b = tmp.b;
				// don't copy alpha, legacy support uses renderamt
			}
			else
			{
				build.m_DiffuseModulation.r = build.m_DiffuseModulation.g = build.m_DiffuseModulation.b = 255;
			}

			// Get the per-instance render-alpha for this static prop
			const char *pAlphaKey = ValueForKey( &entities[i], "renderamt" );
			if ( *pAlphaKey != '\0' )
			{
				build.m_DiffuseModulation.a = Q_atoi( pAlphaKey );
			}
			else
			{
				build.m_DiffuseModulation.a = 255;
			}

			if ( bDoStaticPropCombine )
			{
				vecBuilds.AddToTail( build );
			}
			else
			{
				AddStaticPropToLump( build );
			}

			// strip this ent from the .bsp file
			entities[i].epairs = 0;
		}
	}

	if ( bDoStaticPropCombine )
	{
		int nCombinePropCount = 0;

		Msg( "\nCombining static props to reduce drawcalls...\n\n" );

		int nOriginalPropCount = vecBuilds.Count();

		if ( staticpropcombine_autocombine || staticpropcombine_suggestcombinerules )
		{
			AutoCreatePropCombineRules( vecBuilds );
		}

		propBuildVector vecCombinedStaticProps;
		while( vecBuilds.Count() )
		{

			//color32 tempColorize = { 0, 255, 0, 255 };
			//tempColorize.r = (byte)(RandomInt(1,10)*25);
			//tempColorize.g = (byte)(RandomInt(1,10)*25);
			//tempColorize.b = (byte)(RandomInt(1,10)*25);

			propBuildVector vecGroup;
			while ( GroupProp( vecBuilds, vecGroup ) ) {}

			if ( vecGroup.Count() > 0 )
			{
				if ( vecGroup.Count() >= g_nAutoCombineMinInstances )
				{
					// set the fade min/max to that of the furthest group member
					float flFadeMinDist = 0;
					float flFadeMaxDist = 0;

					int nCount_STATIC_PROP_IGNORE_NORMALS = 0;
					int nCount_STATIC_PROP_NO_SHADOW = 0;
					int nCount_STATIC_PROP_NO_FLASHLIGHT = 0;
					int nCount_STATIC_PROP_MARKED_FOR_FAST_REFLECTION = 0;
					int nCount_STATIC_PROP_NO_PER_VERTEX_LIGHTING = 0;
					int nCount_STATIC_PROP_NO_SELF_SHADOWING = 0;
					int nCount_STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH = 0;

					FOR_EACH_VEC( vecGroup, n )
					{
						flFadeMinDist = MAX( flFadeMinDist, vecGroup[n].m_FadeMinDist );
						flFadeMaxDist = MAX( flFadeMaxDist, vecGroup[n].m_FadeMaxDist );

						#define HelperCheckFlags( _flagname, _flagbits ) if ( vecGroup[n]._flagbits & _flagname ) { nCount_##_flagname++; }

						HelperCheckFlags( STATIC_PROP_IGNORE_NORMALS, m_Flags );
						HelperCheckFlags( STATIC_PROP_NO_SHADOW, m_Flags );
						HelperCheckFlags( STATIC_PROP_NO_FLASHLIGHT, m_Flags );
						HelperCheckFlags( STATIC_PROP_MARKED_FOR_FAST_REFLECTION, m_Flags );
						HelperCheckFlags( STATIC_PROP_NO_PER_VERTEX_LIGHTING, m_Flags );
						HelperCheckFlags( STATIC_PROP_NO_SELF_SHADOWING, m_Flags );
						HelperCheckFlags( STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH, m_FlagsEx );

						#undef HelperCheckFlags
					}

					FOR_EACH_VEC( vecGroup, n )
					{
						vecGroup[n].m_FadeMinDist = flFadeMinDist;
						vecGroup[n].m_FadeMaxDist = flFadeMaxDist;

						#define HelperSetFlags_IF_ALL( _flagname, _flagbits ) if ( nCount_##_flagname == vecGroup.Count() ) { vecGroup[n]._flagbits |= _flagname; } else { vecGroup[n]._flagbits &= ~_flagname; }
						#define HelperSetFlags_IF_ANY( _flagname, _flagbits ) if ( nCount_##_flagname > 0 ) { vecGroup[n]._flagbits |= _flagname; } else { vecGroup[n]._flagbits &= ~_flagname; }

						HelperSetFlags_IF_ALL( STATIC_PROP_IGNORE_NORMALS, m_Flags );
						HelperSetFlags_IF_ALL( STATIC_PROP_NO_SHADOW, m_Flags );
						HelperSetFlags_IF_ALL( STATIC_PROP_NO_FLASHLIGHT, m_Flags );
						HelperSetFlags_IF_ANY( STATIC_PROP_MARKED_FOR_FAST_REFLECTION, m_Flags );
						HelperSetFlags_IF_ALL( STATIC_PROP_NO_PER_VERTEX_LIGHTING, m_Flags );
						HelperSetFlags_IF_ALL( STATIC_PROP_NO_SELF_SHADOWING, m_Flags );
						HelperSetFlags_IF_ANY( STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH, m_FlagsEx );

						#undef HelperSetFlags_IF_ALL
						#undef HelperSetFlags_IF_ANY
					}

					CombineStaticProps( vecGroup, nCombinePropCount );
					//vecGroup[0].m_DiffuseModulation = tempColorize;

					vecCombinedStaticProps.AddToTail( vecGroup[ 0 ] );
				}
				else
				{
					for ( int i = 0; i < vecGroup.Count(); i++ )
					{
						vecCombinedStaticProps.AddToTail( vecGroup[ i ] );
					}
				}
			}
			else
			{
				vecCombinedStaticProps.AddToTail( vecBuilds[0] );
				vecBuilds.Remove(0);
			}

		}

		Msg( "\nCompleted static prop combine.\n" );
		
		FOR_EACH_VEC( g_vecCombineRules, i )
		{
			if ( g_vecCombineRules[i].m_vecStatsMemberCounts.Count() )
			{
				int nSum = 0;
				FOR_EACH_VEC( g_vecCombineRules[i].m_vecStatsMemberCounts, j )
				{
					nSum += g_vecCombineRules[i].m_vecStatsMemberCounts[j];
				}
				float flAvg = ((float)(nSum)) / ((float)(g_vecCombineRules[i].m_vecStatsMemberCounts.Count()));

				Msg( "%i clusters of group \"%s\",\taverage %.1f models\n",
					g_vecCombineRules[i].m_vecStatsMemberCounts.Count(),
					g_vecCombineRules[i].m_szGroupName.Get(),
					flAvg );
			}
				
		}

		int nPropRemovedCount = nOriginalPropCount - vecCombinedStaticProps.Count();
		
		Msg( "Props combined away: %i\n", nPropRemovedCount );
		Msg( "Cluster models built: %i\n\n", nCombinePropCount );

		FOR_EACH_VEC( vecCombinedStaticProps, i )
		{
			AddStaticPropToLump( vecCombinedStaticProps[i] );
		}
	}

	// Strip out lighting origins; has to be done here because they are used when
	// static props are made
	for ( i = s_LightingInfo.Count(); --i >= 0; )
	{
		// strip this ent from the .bsp file
		entities[s_LightingInfo[i]].epairs = 0;
	}


	SetLumpData( );
}

static studiohdr_t *g_pActiveStudioHdr;
static void SetCurrentModel( studiohdr_t *pStudioHdr )
{
	// track the correct model
	g_pActiveStudioHdr = pStudioHdr;
}

static void FreeCurrentModelVertexes()
{
	Assert( g_pActiveStudioHdr );

	if ( g_pActiveStudioHdr->VertexBase() )
	{
		free( g_pActiveStudioHdr->VertexBase() );
		g_pActiveStudioHdr->SetVertexBase( NULL );
	}
}

const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void * pModelData )
{
	studiohdr_t *pActiveStudioHdr = static_cast<studiohdr_t *>(pModelData);

	if ( pActiveStudioHdr->VertexBase() )
	{
		return (vertexFileHeader_t *)pActiveStudioHdr->VertexBase();
	}

	// mandatory callback to make requested data resident
	// load and persist the vertex file
	char				fileName[260];
	strcpy( fileName, "models/" );	
	strcat( fileName, pActiveStudioHdr->pszName() );
	Q_StripExtension( fileName, fileName, sizeof( fileName ) );
	strcat( fileName, ".vvd" );

	// load the model
	CUtlBuffer bufFileData;
	if ( !GetMapDataFilesMgr()->ReadRegisteredFile( fileName, bufFileData ) &&
		 !g_pFileSystem->ReadFile( fileName, NULL, bufFileData ) )
	{
		if ( V_stristr( fileName, "_autocombine_" ) && FileExistsInPak( GetPakFile(), fileName ) )
		{
			if ( !ReadFileFromPak( GetPakFile(), fileName, false, bufFileData ) )
				Error( "Unable to load vertex data \"%s\"\n", fileName );
		}
		else
		{
			Error( "Unable to load vertex data \"%s\"\n", fileName );
		}
	}

	// Get the file size
	int vvdSize = bufFileData.TellPut();
	if (vvdSize == 0)
	{
		Error( "Bad size for vertex data \"%s\"\n", fileName );
	}

	vertexFileHeader_t *pVvdHdr = (vertexFileHeader_t *) bufFileData.Base();

	// check header
	if ( pVvdHdr->id != MODEL_VERTEX_FILE_ID )
	{
		Error("Error Vertex File %s id %d should be %d\n", fileName, pVvdHdr->id, MODEL_VERTEX_FILE_ID);
	}
	if ( pVvdHdr->version != MODEL_VERTEX_FILE_VERSION )
	{
		Error("Error Vertex File %s version %d should be %d\n", fileName, pVvdHdr->version, MODEL_VERTEX_FILE_VERSION);
	}
	if ( pVvdHdr->checksum != pActiveStudioHdr->checksum )
	{
		Error("Error Vertex File %s checksum %d should be %d\n", fileName, pVvdHdr->checksum, pActiveStudioHdr->checksum);
	}

	// need to perform mesh relocation fixups
	// allocate a new copy
	vertexFileHeader_t *pNewVvdHdr = (vertexFileHeader_t *)malloc( vvdSize );
	if ( !pNewVvdHdr )
	{
		Error( "Error allocating %d bytes for Vertex File '%s'\n", vvdSize, fileName );
	}

	// load vertexes and run fixups
	bool bExtraData = (pActiveStudioHdr->flags & STUDIOHDR_FLAGS_EXTRA_VERTEX_DATA) != 0;
	Studio_LoadVertexes(pVvdHdr, pNewVvdHdr, 0, true, bExtraData);

	// discard original
	pVvdHdr = pNewVvdHdr;

	pActiveStudioHdr->SetVertexBase( (void*)pVvdHdr );
	return pVvdHdr;
}
