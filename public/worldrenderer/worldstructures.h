//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef WORLDSTRUCTURES_H
#define WORLDSTRUCTURES_H

#ifdef _WIN32
#pragma once
#endif

#define WORLD_FILE_VERSION 101

#include "tier0/platform.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"
#include "bvhsupport.h"

//--------------------------------------------------------------------------------------
// Chunk types
//--------------------------------------------------------------------------------------
enum BVHChunkType_t
{
	// Client-only resources
	// GPU resources
	CHUNK_TYPE_RESOURCE_DICTIONARY			= 0,						// Resource dictionary
	CHUNK_TYPE_ENTITIES,												// Entity lump
	CHUNK_TYPE_HEIRARCHY,												// Heirarchy
	CHUNK_TYPE_VISIBILITY,												// Visibility vectors

	MAX_CHUNK_TYPES
};

//--------------------------------------------------------------------------------------
// Generic chunk descriptor
//--------------------------------------------------------------------------------------
struct BVHChunkDescriptor_t
{
	DECLARE_BYTESWAP_DATADESC();

	BVHChunkType_t							m_nChunkType;				// type for the chunk
	uint64									m_nOffset;					// Offset from the beginning of the file to this chunk
	uint64									m_nSize;					// Number of bytes taken up
};

//--------------------------------------------------------------------------------------
// Fake-material related
//--------------------------------------------------------------------------------------
enum ShaderComboVariation_t
{
	VARIATION_DEFAULT						= 0,						// business as usual
	VARIATION_DEPTH,													// depth only
	VARIATION_NORM_DEPTH_SPEC,											// normal and specular power
	VARIATION_SAMPLE_LIGHTING,											// sampling lighting
	VARIATION_SAMPLE_LIGHTING_LOW_TEXTURE,								// sampling lighting and only add a little texture in

	// Tools modes
	VARIATION_BAKE_ALL,													// bake albedo, normal, and specular
	VARIATION_RENDER_UV,												// Render position and normal to UV space

	MAX_VARIATIONS
};

#define MAX_RUNTIME_VARIATIONS VARIATION_BAKE_ALL

//--------------------------------------------------------------------------------------
// Resource related
//--------------------------------------------------------------------------------------
enum BVHResourceType_t
{
	// Client-only resources
	// GPU resources
	BVH_VERTEX_BUFFER						= 0,
	BVH_CONSTANT_BUFFER,
	BVH_INDEX_BUFFER,
	BVH_TEXTURE,
	BVH_PRECOMP_CMD_BUFFER,												// 360-only for now

	// Cutoff for client-only resources
	BVH_MAX_CLIENT_ONLY_RESOURCES			= 16,						// Make this a hard line in the sand?

	// Non-client-only resources
	BVH_MATERIAL,														// Fake material class
	BVH_KDTREE,															// KD-tree for raycasts (normally stored in node0)
	BVH_POINTLIGHT_DATA,												// pointlight data
	BVH_HEMILIGHT_DATA,													// hemilight data
	BVH_SPOTLIGHT_DATA,													// spotlight data
	BVH_PVS,
	BVH_SOMETHING_ELSE,
};

//--------------------------------------------------------------------------------------
// use this buffer desc instead of BufferDesc_t because BufferDesc_t has pointers
// that won't serialized consistently between 32 and 64bits
//--------------------------------------------------------------------------------------
struct BVHBufferDesc_t
{
	DECLARE_BYTESWAP_DATADESC();

	RenderBufferType_t						m_nBufferType;
	int32									m_nElementCount;			// Number of vertices/indices
	int32									m_nElementSizeInBytes;		// Size of a single vertex/index
};

//--------------------------------------------------------------------------------------
// Dictionary related
//--------------------------------------------------------------------------------------
enum BVHDictionaryEntryFlags_t
{
	NON_PAGEABLE							= 0x01,						// Resource can never be evicted once loaded
	USE_SYS_MEMORY							= 0x02,						// Use system memory to fulfill this request
	USE_INSTANCE_DATA						= 0x04,						// Instance data is in this resource
	//USE_GPU_MEMORY						= 0x04,						// Use GPU visible memory to fulfill this request
	USE_TRANSIENT_MEMORY					= 0x08,						// Use transient memory ( reclaimed system memory )
};

#define MAX_RESOURCE_NAME 64
class CBVHDictionaryEntry
{
public:
	DECLARE_BYTESWAP_DATADESC();

	CBVHDictionaryEntry() :
	  m_nRefCount( 0 )
	{
	}
	~CBVHDictionaryEntry() 
	{
	}

	int AddRef() 
	{ 
		return ++m_nRefCount; 
	}
	int Release()
	{
		m_nRefCount--;
		if ( m_nRefCount == 0 )
		{
			// TODO: do we need to do anything here?
		}

		return m_nRefCount;
	}
	int GetRefCount()
	{
		return m_nRefCount;
	}

	uint8 GetFlags()						{ return m_Flags; }
	uint64 GetOffset() const				{ return m_ChunkDesc.m_nOffset; }
	uint64 GetSize()						{ return m_ChunkDesc.m_nSize; }
	int GetResourceType()					{ return m_nResourceType; }
	char *GetName()							{ return m_pName; }
	bool GetInstanced()						{ return m_bInstanceData; }
	void SetFlags( unsigned char Flags ) { m_Flags = Flags; }
	void SetOffset( uint64 offset )			{ m_ChunkDesc.m_nOffset = offset; }
	void SetSize( uint64 size )				{ m_ChunkDesc.m_nSize = size; }
	void SetResourceType( int type )		{ m_nResourceType = type; }
	void SetName( char *pName )				{ Q_strncpy( m_pName, pName, MAX_RESOURCE_NAME ); }
	void SetInstanced( bool bInstanced )	{ m_bInstanceData = bInstanced; }
private:

	BVHChunkDescriptor_t					m_ChunkDesc;				// Chunk descriptor for the mapping in file
	int32									m_nRefCount;				// Reference count
	uint32									m_nLastFrameUsed;			// The last frame in which this resource was used
	int32									m_nResourceType;			// of type BVHResourceType_t
	char									m_pName[MAX_RESOURCE_NAME];	// unique name to help identify this resource or its origin

	uint8									m_Flags;					// of type DictionaryEntryFlags_t
	bool									m_bInstanceData;			// is this data instanced?
	uint8									m_padding[2];				// pad the struct out to a multiple of 4 bytes
};

#define MAX_PAGE_FILE_NAME 64
struct BVHResourceDictionaryHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	int32									m_nInputLayouts;			// Number of pre-defined input layouts for this map
	int32									m_nResources;				// Number of instanced resources
	char									m_pPageFile[MAX_PAGE_FILE_NAME]; // name of our page file
};

//--------------------------------------------------------------------------------------
// Input layout
//--------------------------------------------------------------------------------------
struct BVHInputLayoutDesc_t
{
	DECLARE_BYTESWAP_DATADESC();

	char									m_pName[RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE];
	int32									m_nFields;
	union 
	{
		RenderInputLayoutField_t			*m_pFields;
		uint64								m_64Bits;  // force to 64bits
	};
};

//--------------------------------------------------------------------------------------
// Draw-call related
//--------------------------------------------------------------------------------------
struct BVHResourceBinding_t
{
	DECLARE_BYTESWAP_DATADESC();

	int32									m_nResourceIndex;			// Which resource in the resource dictionary to bind
	int32									m_nBindOffset;				// Byte offset for binding (only applicable for buffers)
	int32									m_nElementStride;			// Element stride (only applicable for buffers) (byte?, short?)
	uint8									m_cBindStage;				// Which stage to bind to (IA,SHADER)
	uint8									m_cBindSlot;
	uint8									m_padding[2];				// pad this structure out to a multiple of 4 bytes
};

enum BVHDrawCallFlags_t
{
	DRAW_WHEN_NODE_LOADED 					= 0x00000001,				// Wait for the entire node to load before drawing.  Mutex with ASAP.
	DRAW_ASAP 								= 0x00000002,				// Draw as soon as we're loaded.  Don't wait for anything.
	DRAW_IF_PREVIOUS_DREW					= 0x00000004,				// Only draw if our previous draw happened
	DRAW_TRANSPARENT 						= 0x00000008,				// We have transparency
	DRAW_WHEN_CAMERA_INSIDE					= 0x00000010,				// Only draw when the camera is in this node
	DRAW_ALWAYS								= 0x00000020,				// Draw if any of the traversal hits this node
	DRAW_CULL								= 0x00000040,				// Cull the draw individually using the draw's bounding box
	DRAW_LIGHT								= 0x00000080,				// The draw call is bounding light geometry
};

enum BVHBindStage_t
{
	BIND_STAGE_MATERIAL						= 0x0,
	BIND_STAGE_SHADER,
	BIND_STAGE_IA,
};

class CBVHDrawCall
{
public:
	bool Issue();														// issue the draw call

	int32 GetFlags()						{ return m_Flags; }
	int32 GetInputLayout()					{ return m_nInputLayout; }
	int32 GetNumResourceBindings()			{ return m_nResourceBindings; }

public:
	DECLARE_BYTESWAP_DATADESC();

	int32									m_Flags;					// One of BVHDrawCallFlags_t
	
	AABB_t									m_Bounds;					// Bounding box for culling

	int32									m_nInputLayout;				// Index for the pre-defined input layout for the vertices
	int32									m_nResourceBindings;		// Number of resource bindings

	// Draw call data
	RenderPrimitiveType_t					m_nPrimitiveType;			// Type of primitive to draw
	int32									m_nBaseVertex;				// Base vertex to use when rendering
	int32									m_nVertexCount;				// Number of vertices
	int32									m_nStartIndex;				// First index to use
	int32									m_nIndexCount;				// Number of indices to draw ( or index count per instance if instancing )
	int32									m_nStartInstance;			// Location of the first instance
	int32									m_nInstanceCount;			// Number of instances ( if 0, instancing is disabled )

	// Binding pointer stored in-file
	union // TODO: are nameless unions ok in gcc?
	{
		BVHResourceBinding_t				*m_pResourceBindings;		// Resource bindings
		uint64								m_64Bits;					// pad to 64bits
	};
};

//--------------------------------------------------------------------------------------
// BVHNode related
//--------------------------------------------------------------------------------------
enum BVHNodeFlags_t
{
	NODE_SIMPLIFIED							= 0x0001,					// The geometry is simplified
	NODE_UNIQUE_UV							= 0x0002,					// The geometry is uniquely mapped (likely, we're a higher LOD)
	NODE_ATLASED							= 0x0004,					// This node was atlased but not uniquely mapped
	NODE_KDTREE								= 0x0008,					// Node contains a kd-tree for raycasts
	NODE_NODRAW								= 0x0010,					// Node has no actual draw calls... it's just a container for stuff and other nodes
	NODE_START_TRAVERSAL					= 0x0020,					// Start a traversal at this node (add a check to ensure that the KDTREE flag also exists with this one)
	NODE_CAN_SEE_SKY						= 0x0040,					// Can this node see the sky?
	NODE_MOST_DETAILED						= 0x0080,					// Node is the most detailed node containing the original geometry and textures
};

struct BVHNodeHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	int32									m_nID;						// Node ID
	int32									m_Flags;					// One of BVHNodeFlags_t

	// hierarchy
	int32									m_nParent;					// Parent node
	TiledPosition_t							m_Origin;					// Tiled-position origin placing us in the world
	AABB_t									m_Bounds;					// Axis-aligned bounding-box (pull out and vectorize?)
	float									m_flMinimumDistance;		// Minimum camera distance at which this node renders (pull out and vectorize?)
	int32									m_nChildren;				// Number of child nodes

	// resources
	int32									m_nResources;				// Number of resources needed by this node (int16?)
	int32									m_nDrawCalls;				// Number of draw calls (int16?)
}; 

//--------------------------------------------------------------------------------------
// World related
//--------------------------------------------------------------------------------------
struct BVHBuilderParams_t
{
	DECLARE_BYTESWAP_DATADESC();

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

//--------------------------------------------------------------------------------------
// File header
//--------------------------------------------------------------------------------------
struct WorldFileHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	uint32									m_nFileVersion;				// Versioning
	Vector									m_vWorldUnitsPerTile;		// World units per-tile that this map was built against
	int32									m_nChunks;					// Number of chunks

	BVHBuilderParams_t						m_BuilderParams;			// Original build parameters ( so we can potentially remake this file )
};

//--------------------------------------------------------------------------------------
// Known chunk headers
//--------------------------------------------------------------------------------------
struct HierarchyChunkHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	int32									m_nNodes;					// Number of nodes in this world
	int32									m_nMaxNodeSizeBytes;		// Maximum size of a node in bytes (Average instead?)
	int32									m_nAvgNodeSizeBytes;		// Average size of a node in bytes (Average instead?)
};

struct EntityChunkHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	int32									m_nEntities;				// Entity count
};

struct VisibilityChunkHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	int32									m_nNodes;					// Num nodes accounted for
	int32									m_nDWORDS;					// Num dwords per visibility chunk
	int32									m_nX;						// Num grid points in X
	int32									m_nY;						// Num grid points in Y
	int32									m_nZ;						// Num grid points in Z
	Vector									m_vCellSize;				// Cell size
	Vector									m_vStart;					// Cell start
};

struct RenderInputLayoutFieldProxy_t
{
	DECLARE_BYTESWAP_DATADESC();

	char m_pSemanticName[RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE];
	int m_nSemanticIndex;
	ColorFormat_t m_Format;
	int m_nOffset;
	int m_nSlot;
	RenderSlotType_t m_nSlotType;
	int m_nInstanceStepRate;
};

//--------------------------------------------------------------------------------------
// Light data
//--------------------------------------------------------------------------------------
struct PointLightData_t
{
	Vector m_vOrigin;
	Vector4D m_vColorNRadius;
	Vector m_vAttenuation;
};

struct HemiLightData_t
{
	Vector4D m_vTransform0;		// Direction is z column
	Vector4D m_vTransform1;		// Direction is z column
	Vector4D m_vTransform2;		// Direction is z column
	Vector4D m_vColorNRadius;
	Vector m_vAttenuation;
};

struct SpotLightData_t
{
	Vector4D m_vTransform0;		// Direction is z column
	Vector4D m_vTransform1;		// Direction is z column
	Vector4D m_vTransform2;		// Direction is z column
	Vector4D m_vColorNRadius;
	Vector4D m_vAttenuationNCosSpot;
};

#endif
