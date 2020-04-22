//============ Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===============================================================================//
#ifndef WORLD_SCHEMA_H
#define WORLD_SCHEMA_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "worldnodeschema.h"

//--------------------------------------------------------------------------------------
// Enum related
//--------------------------------------------------------------------------------------
enum WorldNodeFlags_t
{
	WORLD_NODE_SIMPLIFIED						= 0x0001,					// The geometry is simplified
	WORLD_NODE_UNIQUE_UV						= 0x0002,					// The geometry is uniquely mapped (likely, we're a higher LOD)
	WORLD_NODE_ATLASED							= 0x0004,					// This node was atlased but not uniquely mapped
	WORLD_NODE_KDTREE							= 0x0008,					// Node contains a kd-tree for raycasts
	WORLD_NODE_NODRAW							= 0x0010,					// Node has no actual draw calls... it's just a container for stuff and other nodes
	WORLD_NODE_START_TRAVERSAL					= 0x0020,					// Start a traversal at this node (add a check to ensure that the KDTREE flag also exists with this one)
	WORLD_NODE_CAN_SEE_SKY						= 0x0040,					// Can this node see the sky?
	WORLD_NODE_MOST_DETAILED					= 0x0080,					// Node is the most detailed node containing the original geometry and textures
};

schema struct WorldBuilderParams_t
{
	int32									m_nSizeBytesPerVoxel;		// target size per-voxel	
	float									m_flMinDrawVolumeSize;		// minimum size of any draw call
	float									m_flMinDistToCamera;		// minimum distance to camera for near objects
	float									m_flMinAtlasDist;			// minimum distance at which any atlased node can be visible
	float									m_flMinSimplifiedDist;		// minimum distance at which any simplified node can be visible
	float									m_flHorzFOV;				// horizontal fov used for texel to screenspace calcs
	float									m_flHalfScreenWidth;		// half target screen res used for texel to screenspace calcs
	int32									m_nAtlasTextureSizeX;		// X res of atlas textures
	int32									m_nAtlasTextureSizeY;		// Y res of atlas textures
	int32									m_nUniqueTextureSizeX;		// X res of uniquely atlased textures
	int32									m_nUniqueTextureSizeY;		// Y res of uniquely atlased textures
	int32									m_nCompressedAtlasSize;		// approx size of a compressed atlas texture
	float									m_flGutterSize;				// gutter size (in texels)
	float									m_flUVMapThreshold;			// cos( angle ) threshold between faces when creating a unique uv parameterization
	Vector									m_vWorldUnitsPerTile;		// world units per tile for tiled coordinates
	int32									m_nMaxTexScaleSlots;		// maximum number of gpu registers we can take up with texture scaling
	bool									m_bWrapInAtlas;				// true == handle wrapping texcoords by tiling the texture in the atlas
																		// false == handle wrapping by a frac in the pixel shader
	uint8									m_padding[3];				// pad this structure out to a mutiple of 4 bytes
};

schema struct NodeData_t
{
	int32									m_Flags;					// One of WorldNodeFlags_t
	int32									m_nParent;					// Parent node index
	Vector									m_vOrigin;					// Origin placing us in the world
	Vector									m_vMinBounds;				// Axis-aligned bounding-box min
	Vector									m_vMaxBounds;				// Axis-aligned bounding-box max
	float									m_flMinimumDistance;		// Minimum camera distance at which this node renders (pull out and vectorize?)
	CResourceArray< int32 >					m_ChildNodeIndices;			// List of indices of the child nodes

	CResourceReference< WorldNode_t >		m_hWorldNode;				// Handle to the world node
};

schema struct World_t
{
	WorldBuilderParams_t					m_builderParams;			// Original build parameters ( so we can potentially remake this file )
	CResourceArray< NodeData_t >			m_worldNodes;				// World nodes
	CResourceString							m_entityString;				// All of the entity text

	// Placeholder for visibility
};

class CWorld;	// Forward declaration of associated runtime class
DEFINE_RESOURCE_CLASS_TYPE( World_t, CWorld, RESOURCE_TYPE_WORLD );
typedef const ResourceBinding_t< CWorld > *HWorld;
typedef CStrongHandle< CWorld > HWorldStrong;
#define WORLD_HANDLE_INVALID ( (HWorld)0 )


#endif // WORLD_SCHEMA_H
