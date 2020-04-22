//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Class to represent and write out a BSP file.
//
//===============================================================================

#ifndef SIMPLEBSPFILE_H
#define SIMPLEBSPFILE_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "bitvec.h"

#include "simplemapfile.h"
#include "vbspmathutil.h"

class CSimpleMapFile;
struct MapEntity_t;

//-----------------------------------------------------------------------------
// Indicates that the BSP node is a leaf (m_pChildren will be all NULL)
//-----------------------------------------------------------------------------
static const int LEAF_PLANE_INDEX = -1;

//-----------------------------------------------------------------------------
// A sentinel cluster index used to indicate that a node does not
// belong to any cluster.
//-----------------------------------------------------------------------------
static const int INVALID_CLUSTER_INDEX = -1;

//-----------------------------------------------------------------------------
// Default maximum visibility radius used to compute cluster-to-cluster
// visibility.
//-----------------------------------------------------------------------------
static const float DEFAULT_VISIBILITY_RADIUS = 2500.0f;

class CBSPBrush;
class CBSPFace;
class CBSPPortal;

//-----------------------------------------------------------------------------
// A node in a BSP tree.
//-----------------------------------------------------------------------------
class CBSPNode
{
public:
	CBSPNode();
	~CBSPNode();

	bool IsLeaf() const { return m_nSplitPlaneIndex == LEAF_PLANE_INDEX; }

	CBSPNode *m_pParent;
	CBSPNode *m_pChildren[2];
	// If this node is a leaf, the value is LEAF_PLANE_INDEX.
	// Otherwise, the split plane is always the "positive" plane of a pair (that is, even numbered).
	int m_nSplitPlaneIndex;
	// Flags indicating the contents of the BSP node.
	MapBrushContentsFlags_t m_ContentsFlags;
	// This field owns the contents' memory
	CUtlVector< CBSPBrush * > m_ClippedBrushes;
	// A list of portal faces or detail faces on this node.
	// Leaf nodes may contain only detail faces, non-leaf nodes may contain only portal faces.
	// This field does NOT own the contents' memory; the faces are owned by the containing model or the portal
	// (depending on what kind of face it is).
	CUtlVector< CBSPFace * > m_Faces;
	// This field does NOT own the contents' memory; the portals are owned by the containing model.
	CUtlVector< CBSPPortal * > m_Portals;
	// AABB around the node
	Vector m_vMinBounds, m_vMaxBounds;
	// True if an entity can reach this node via portals, False if unreachable
	bool m_bEntityCanReach;
	// The BSP cluster to which this leaf node belongs (only applies to the world BSP tree).
	// A cluster is simply a group of leaves which share visibility information.
	// In practice, there is 1 cluster per non-solid world leaf.
	// This value is ignored for non-leaf nodes.
	int m_nClusterIndex;

private:
	// Disallow value semantics
	CBSPNode( const CBSPNode &other );
	CBSPNode &operator=( const CBSPNode &other );
};

//-----------------------------------------------------------------------------
// A binary space partition tree.
//-----------------------------------------------------------------------------
class CBSPTree
{
public:
	CBSPTree();
	~CBSPTree();

	CBSPNode *m_pRoot;
	CBSPNode m_OutsideNode; // a sentinel node which exists in the space outside of the BSP tree
	// AABB around the tree
	Vector m_vMinBounds, m_vMaxBounds;

private:
	// Disallow value semantics
	CBSPTree( const CBSPTree &other );
	CBSPTree &operator=( const CBSPTree &other );
};

//-----------------------------------------------------------------------------
// A portal between two nodes of a BSP tree.
//-----------------------------------------------------------------------------
class CBSPPortal
{
public:
	//-----------------------------------------------------------------------------
	// Constructs a default, null portal.
	//-----------------------------------------------------------------------------
	CBSPPortal();
	//-----------------------------------------------------------------------------
	// Constructs a new portal along the same plane as the given portal,
	// except with a different shape.  
	// This is used when a single portal is cut by a node's splitting plane 
	// into two new portals along the same plane.
	//
	// This constructor swaps its polygon's points with the given polygon's 
	// for efficiency!
	//-----------------------------------------------------------------------------
	CBSPPortal( const CBSPPortal &other, Polygon_t *pPortalShape );
	~CBSPPortal();

	CBSPNode *GetOtherNode( const CBSPNode *pNode ) const;

	//-----------------------------------------------------------------------------
	// Returns either 0 or 1 to indicate which node attached to this portal
	// is responsible for drawing the face. 
	// This call is only valid if m_PortalFaces.Count() > 0.
	//-----------------------------------------------------------------------------
	int GetNodeIndexForFace() const;

	// The nodes on either side of the portal.
	// The convention is that the plane of the portal faces in the direction from node 1 towards node 0.
	// (That is, node 0 is in the positive half-space of the plane while node 1 is in the the negative half-space).
	// In a properly built tree, these nodes are always leaves (during construction they may
	// point to interior nodes).
	CBSPNode *m_pNodes[2];

	// Not NULL if this portal lives on a node's split plane (which is usually the case, except for
	// portals at the edge of the world).  This node is never a leaf.
	CBSPNode *m_pOnNode;

	// Shape of the portal.
	Polygon_t m_Polygon;

	// Plane along which the portal lives.
	Plane_t m_Plane;

	// These are the faces generated when the portal is in-between solid and non-solid contents.
	// Logically, there is only 1 convex face, but some constraints (e.g. max lightmap size per face)
	// can require subdivision of faces.
	// This class owns the faces and frees them on destruction.
	CUtlVector< CBSPFace * > m_PortalFaces;

private:
	// Disallow value semantics
	CBSPPortal( const CBSPPortal &other );
	CBSPPortal &operator=( const CBSPPortal &other );
};

//-----------------------------------------------------------------------------
// The side of a BSP brush (convex solid).
// This class is copyable.
//-----------------------------------------------------------------------------
class CBSPBrushSide
{
public:
	//-----------------------------------------------------------------------------
	// Constructs a default, invalid brush side.
	//-----------------------------------------------------------------------------
	CBSPBrushSide();
	
	//-----------------------------------------------------------------------------
	// Constructs a new brush side initialized from map data.
	//-----------------------------------------------------------------------------
	CBSPBrushSide( const MapBrushSide_t *pMapBrushSide );

	// Index into BSP creation context's plane list
	int m_nPlaneIndex;
	// Index into map's texture infos array
	int m_nTextureInfoIndex;
	MapBrushSideSurfaceFlags_t m_SurfaceFlags;
	Polygon_t m_Polygon;
};

//-----------------------------------------------------------------------------
// A convex solid in a BSP file, produced from CSG operations on brushes
// found in the original map.
// This class is copyable.
//-----------------------------------------------------------------------------
class CBSPBrush
{
public:
	//-----------------------------------------------------------------------------
	// Constructs a default brush solid with no sides.
	//-----------------------------------------------------------------------------
	CBSPBrush();

	//-----------------------------------------------------------------------------
	// Constructs a new default brush solid initialized from map data.
	//-----------------------------------------------------------------------------
	CBSPBrush( const MapBrush_t *pMapBrush, const MapBrushSide_t *pMapBrushSides );
	
	Vector m_vMinBounds, m_vMaxBounds;

	CCopyableUtlVector< CBSPBrushSide > m_Sides;
	MapBrushContentsFlags_t m_ContentsFlags;
	const MapBrush_t *m_pOriginalBrush;

	// Which side of the node splitting plane this brush is on.
	// (both values only used during BSP construction in order to split nodes).
	PlaneSide_t m_nTempSplitSide;
	PlaneSide_t m_nSplitSide;
};

//-----------------------------------------------------------------------------
// A renderable face associated with the side of a detail brush or a portal.
// This class is copyable.
//-----------------------------------------------------------------------------
class CBSPFace
{
public:
	//-----------------------------------------------------------------------------
	// Constructs a default, empty face.
	//-----------------------------------------------------------------------------
	CBSPFace();

	//-----------------------------------------------------------------------------
	// Constructs a face from the side of a BSP brush solid.
	//-----------------------------------------------------------------------------
	CBSPFace( const CBSPBrush *pBrush, const CBSPBrushSide *pSide );

	//-----------------------------------------------------------------------------
	// Constructs a face from another face, except with a different shape.
	// Useful when cutting a face into parts.
	//
	// This constructor swaps its polygon's points with the given polygon's 
	// for efficiency.
	//-----------------------------------------------------------------------------
	CBSPFace( const CBSPFace &other, Polygon_t *pPolygon );

	// Index into map's texture infos array
	int m_nTextureInfoIndex;
	// Index into BSP creation context's plane list
	int m_nPlaneIndex;
	Polygon_t m_Polygon;
	// Index into the map's displacement array, if this face comes from a displacement surface.
	int m_nDisplacementIndex;
	// The index of this face in the final output.
	// All faces in a BSP file appear sequentially by model.  
	// Within a model's contiguous list of faces, portal faces are serialized first 
	// via depth-first traversal of the BSP tree, followed by detail faces.
	int m_nSerializedFaceIndex;
};

//-----------------------------------------------------------------------------
// An entity in a map with associated CSG brush data.
// The 0th model in a BSP file corresponds to the world entity of the map
// and includes all detail brushes from detail entities in the original map.
//-----------------------------------------------------------------------------
class CBSPModel
{
public:
	CBSPModel();
	~CBSPModel();

	CBSPTree *m_pTree;
	// Detail faces are owned by the model and are freed on destruction.
	CUtlVector< CBSPFace * > m_DetailFaces;
	// Portals are owned by the model and are freed on destruction.
	CUtlVector< CBSPPortal * > m_Portals;

	Vector m_vMinBounds, m_vMaxBounds;
	
private:
	// Disallow value semantics
	CBSPModel( const CBSPModel &other );
	CBSPModel &operator=( const CBSPModel &other );
};

//-----------------------------------------------------------------------------
// In theory, a cluster is a set of BSP leaf nodes.
// In practice, it is simply a pointer to a single, existing BSP leaf node.
//-----------------------------------------------------------------------------
struct BSPCluster_t
{
	const CBSPNode *m_pLeafNode;
};

//-----------------------------------------------------------------------------
// An in-memory representation of a BSP file, built from a map file.
// This class requires that the map file remain valid for its lifetime,
// though the map file is accessed in a read-only manner.
//-----------------------------------------------------------------------------
class CSimpleBSPFile
{
public:
	CSimpleBSPFile();
	~CSimpleBSPFile();

	const CSimpleMapFile *GetOriginalMap() const { return m_pMapFile; }

	const CPlaneHash *GetPlaneHash() const { return &m_PlaneHash; }

	const CBSPModel * const *GetModels() const { return m_Models.Base(); }
	int GetModelCount() const { return m_Models.Count(); }

	const CBSPFace * GetDisplacementFaces() const { return m_DisplacementFaces.Base(); }
	int GetDisplacementFaceCount() const { return m_DisplacementFaces.Count(); }
	
	void SetVisibilityRadius( float flRadius ) { m_flVisibilityRadius = flRadius; }
	int GetClusterCount() const { return m_Clusters.Count(); }
	const byte * GetVisibilityData() const { return m_VisibilityData.Base(); }

	//-----------------------------------------------------------------------------
	// Populates this in-memory BSP file object from the given in-memory map file.
	//-----------------------------------------------------------------------------
	void CreateFromMapFile( const CSimpleMapFile *pMapFile );

private:
	// Source map file; must remain valid for the lifetime of this object.
	const CSimpleMapFile *m_pMapFile;

	// Array of planes with acceleration structure for fast lookup.
	// Starts off copied from the map (so all map plane indices are valid) but is expanded
	// during the BSP process via CSG operations.
	CPlaneHash m_PlaneHash;
	CUtlVector< CBSPModel * > m_Models;
	CUtlVector< CBSPFace > m_DisplacementFaces;
	CUtlVector< BSPCluster_t > m_Clusters;
	CUtlVector< byte > m_VisibilityData;
	
	// A monotonically increasing value which increments for every face generated during the BSP construction process.
	int m_nNextFaceIndex;

	// The radius value beyond which clusters cannot see each other.
	float m_flVisibilityRadius;
	
	void ProcessWorldEntity( const MapEntity_t *pMapEntity );
	void ProcessEntity( const MapEntity_t *pMapEntity );
	
	CBSPNode *GenerateBSPGrid( int nMinX, int nMinY, int nMaxX, int nMaxY, int nAbsoluteMins[2], int nAbsoluteMaxs[2], CBSPNode **ppNodeList );
	CBSPNode *CreateGridNode( int nX, int nY );

	void CreateBSPBrushList( const MapBrush_t *pBrushes, int nBrushCount, bool bIncludeDetail, bool bIncludeStructural, const Vector &vClipMin, const Vector &vClipMax, CUtlVector< CBSPBrush * > *pBrushList );
	CBSPBrush *CreateClippedBrush( const MapBrush_t *pMapBrush, const Vector &vClipMin, const Vector &vClipMax, int nClipMinPlanes[2], int nClipMaxPlanes[2] );
	
	void SplitBrush( CBSPBrush *pBrush, int nPlaneIndex, CBSPBrush **ppFrontBrush, CBSPBrush **ppBackBrush );
	PlaneSide_t GetPrimaryPlaneSide( CBSPBrush *pBrush, Plane_t *pPlane );

	CBSPTree *BuildBSPTree( CUtlVector< CBSPBrush * > *pBrushList, const Vector &vMin, const Vector &vMax );
	void BuildBSPChildren( CBSPNode *pNode, CUtlVector< CBSPBrush * > *pBrushList );
	int FindBestSplitPlane( CUtlVector< CBSPBrush * > *pBrushList );
	void MakeBrushPolygons( CBSPBrush *pBrush );
	void SplitBrushList( CUtlVector< CBSPBrush * > *pBrushList, CBSPNode *pNode, CUtlVector< CBSPBrush * > *pFrontChildList, CUtlVector< CBSPBrush * > *pBackChildList );
	PlaneSide_t TestBrushAgainstPlaneIndex( CBSPBrush *pBrush, int nPlaneIndex, int *nNumSplits );
	void PruneNodes( CBSPNode *pNode, CUtlVector< CBSPPortal * > *pPortalList );
	
	void BuildTreePortals( CBSPTree *pTree, CUtlVector< CBSPPortal * > *pPortalList );
	void BuildNodePortals( CBSPNode *pNode, CUtlVector< CBSPPortal * > *pPortalList );
	void FloodEntities( CBSPTree *pTree );
	void FloodEntity( CBSPNode *pNode, const Vector &vPosition );
	void FloodFillThroughPortals( CBSPNode *pNode );
	void MakeUnreachableNodesSolid( CBSPNode *pNode );
	void MakeFacesFromPortals( CBSPModel *pModel );
	void NumberPortalFaces( CBSPNode *pNode );
	
	void BuildRadialVisibilityData( CBSPNode *pRootNode );
	void AssignClusterIndicesToLeaves( CBSPNode *pNode );

	void CreateDisplacementFaces();

	void PopulateTreeWithDetail( const CBSPTree *pTree, int nFirstBrush, int nNumBrushes, CUtlVector< CBSPFace * > *pDetailFaceList );
	void FilterFaceIntoTree( CBSPNode *pNode, CBSPFace *pClippedFace, CBSPFace *pOriginalFace );
	void FilterBrushIntoTree( CBSPNode *pNode, CBSPBrush *pBrush );
};

//-----------------------------------------------------------------------------
// Dumps useful information about the BSP to several files beginning
// with the path string pPrefixName and suffixed appropriately
// (e.g. "_ent.txt", "_bsp.csv", "_tex.csv").
//-----------------------------------------------------------------------------
void DumpBSPInfo( const char *pPrefixName, byte *pBSPData, int nBSPDataSize );

//-----------------------------------------------------------------------------
// Writes all faces to a .gl file which can be viewed in GLView.
//-----------------------------------------------------------------------------
void WriteGLBSPFile( FileHandle_t fileHandle, byte *pBSPData, int nBSPDataSize );

//-----------------------------------------------------------------------------
// Dumps the contents of the given lump to the specified file.
//-----------------------------------------------------------------------------
void DumpLump( FileHandle_t fileHandle, byte *pBSPData, int nBSPDataSize, int nLumpIndex );


#endif // SIMPLEBSPFILE_H