//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a world node
//
//=============================================================================

#ifndef DMEWORLD_H
#define DMEWORLD_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "dmeworldnode.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Parameters used to construct the world
//-----------------------------------------------------------------------------
class CDmeWorldBuilderParams : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorldBuilderParams, CDmElement );

public:
	CDmaVar< int32 >						m_nSizeBytesPerVoxel;		// target size per-voxel	
	CDmaVar< float >						m_flMinDrawVolumeSize;		// minimum size of any draw call
	CDmaVar< float >						m_flMinDistToCamera;		// minimum distance to camera for near objects
	CDmaVar< float >						m_flMinAtlasDist;			// minimum distance at which any atlased node can be visible
	CDmaVar< float >						m_flMinSimplifiedDist;		// minimum distance at which any simplified node can be visible
	CDmaVar< float >						m_flHorzFOV;				// horizontal fov used for texel to screenspace calcs
	CDmaVar< float >						m_flHalfScreenWidth;		// half target screen res used for texel to screenspace calcs
	CDmaVar< int32 >						m_nAtlasTextureSizeX;		// X res of atlas textures
	CDmaVar< int32 >						m_nAtlasTextureSizeY;		// Y res of atlas textures
	CDmaVar< int32 >						m_nUniqueTextureSizeX;		// X res of uniquely atlased textures
	CDmaVar< int32 >						m_nUniqueTextureSizeY;		// Y res of uniquely atlased textures
	CDmaVar< int32 >						m_nCompressedAtlasSize;		// approx size of a compressed atlas texture
	CDmaVar< float >						m_flGutterSize;				// gutter size (in texels)
	CDmaVar< float >						m_flUVMapThreshold;			// cos( angle ) threshold between faces when creating a unique uv parameterization
	CDmaVar< Vector >						m_vWorldUnitsPerTile;		// world units per tile for tiled coordinates
	CDmaVar< int32 >						m_nMaxTexScaleSlots;		// maximum number of gpu registers we can take up with texture scaling
	CDmaVar< bool >							m_bWrapInAtlas;				// true == handle wrapping texcoords by tiling the texture in the atlas
};

//-----------------------------------------------------------------------------
// Bounds for a node
//-----------------------------------------------------------------------------
class CDmeNodeData : public CDmElement
{
	DEFINE_ELEMENT( CDmeNodeData, CDmElement );

public:
	CDmaVar< int32 >						m_nID;
	CDmaVar< int32 >						m_Flags;
	CDmaVar< int32 >						m_nParent;
	CDmaVar< Vector >						m_vOrigin;
	CDmaVar< Vector >						m_vMinBounds;
	CDmaVar< Vector >						m_vMaxBounds;
	CDmaVar< float >						m_flMinimumDistance;
	CDmaArray< int32 >						m_ChildNodeIndices;
};

//-----------------------------------------------------------------------------
// World node reference
//-----------------------------------------------------------------------------
class CDmeWorldNodeReference : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorldNodeReference, CDmElement );

public:
	CDmaString								m_worldNodeFileName;
	CDmaElement< CDmeNodeData >				m_nodeData;
};

//-----------------------------------------------------------------------------
// The whole world in DME format
//-----------------------------------------------------------------------------
class CDmeWorld : public CDmElement
{
	DEFINE_ELEMENT( CDmeWorld, CDmElement );

public:
	CDmaElement< CDmeWorldBuilderParams >	m_builderParams;
	CDmaElementArray< CDmeWorldNodeReference >	m_worldNodes;
	CDmaString								m_entityString;				// All of the entity text
};

#endif // DMEWORLD_H