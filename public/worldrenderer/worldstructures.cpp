//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Byteswapping datadescs for the corresponding worldstructures.  These
// must stay in sync with the stucts in worldstructures.h.
//
//===========================================================================//
#include "worldstructures.h"

//--------------------------------------------------------------------------------------
// Fake-material related
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( MaterialResourceBinding_t )
	DEFINE_FIELD( m_cBindStage,				FIELD_CHARACTER ),
	DEFINE_FIELD( m_cBindSlot,				FIELD_CHARACTER ),
	DEFINE_FIELD( m_cBindSampler,			FIELD_CHARACTER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( Material_t )
	DEFINE_ARRAY( m_szShaderVS,				FIELD_CHARACTER, MAX_SHADER_NAME ),
	DEFINE_ARRAY( m_szShaderPS,				FIELD_CHARACTER, MAX_SHADER_NAME ),

	DEFINE_FIELD( m_nBinds,					FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( m_Binds,			MAX_BINDS ),
	DEFINE_FIELD( m_bAlphaTest,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bInstanced,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUseAtlas,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bVertexColor,			FIELD_BOOLEAN ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// Tiled coordinate
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( IntVector )
	DEFINE_FIELD( x,						FIELD_INTEGER ),
	DEFINE_FIELD( y,						FIELD_INTEGER ),
	DEFINE_FIELD( z,						FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( TiledPosition_t )
	DEFINE_EMBEDDED( m_vTile ),
	DEFINE_FIELD( m_vLocal,					FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// AABB
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( AABB_t )
	DEFINE_FIELD( m_vMinBounds,				FIELD_VECTOR ),
	DEFINE_FIELD( m_vMaxBounds,				FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// Generic chunk descriptor
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( BVHChunkDescriptor_t )
	DEFINE_FIELD( m_nChunkType,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nOffset,				FIELD_INTEGER64 ),	// TODO: Add int64 to datamap.h
	DEFINE_FIELD( m_nSize,					FIELD_INTEGER64 ),  // TODO: Add int64 to datamap.h
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// use this buffer desc instead of BufferDesc_t because BufferDesc_t has pointers
// that won't serialized consistently between 32 and 64bits
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( BVHBufferDesc_t )
	DEFINE_FIELD( m_nBufferType,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nElementCount,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nElementSizeInBytes,	FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// Dictionary related
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( CBVHDictionaryEntry )
	DEFINE_EMBEDDED( m_ChunkDesc ),
	DEFINE_FIELD( m_nRefCount,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nLastFrameUsed,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nResourceType,			FIELD_INTEGER ),
	DEFINE_ARRAY( m_pName,					FIELD_CHARACTER, MAX_RESOURCE_NAME ),
	DEFINE_FIELD( m_Flags,					FIELD_CHARACTER ),
	DEFINE_FIELD( m_bInstanceData,			FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_padding,				FIELD_CHARACTER, 2 ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( BVHResourceDictionaryHeader_t )
	DEFINE_FIELD( m_nInputLayouts,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nResources,				FIELD_INTEGER ),
	DEFINE_ARRAY( m_pPageFile,				FIELD_CHARACTER, MAX_PAGE_FILE_NAME ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// Input layout
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( BVHInputLayoutDesc_t )
	DEFINE_ARRAY( m_pName,					FIELD_CHARACTER, RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE ),
	DEFINE_FIELD( m_nFields,				FIELD_INTEGER ),
	DEFINE_FIELD( m_64Bits,					FIELD_INTEGER64 ),	// TODO: Add int64 to datamap.h
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// Draw-call related
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( BVHResourceBinding_t )
	DEFINE_FIELD( m_nResourceIndex,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nBindOffset,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nElementStride,			FIELD_INTEGER ),
	DEFINE_FIELD( m_cBindStage,				FIELD_CHARACTER ),
	DEFINE_FIELD( m_cBindSlot,				FIELD_CHARACTER ),
	DEFINE_ARRAY( m_padding,				FIELD_CHARACTER, 2 ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( CBVHDrawCall )
	DEFINE_FIELD( m_Flags,					FIELD_INTEGER ),
	DEFINE_EMBEDDED( m_Bounds ),
	DEFINE_FIELD( m_nInputLayout,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nResourceBindings,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nPrimitiveType,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nBaseVertex,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nVertexCount,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nStartIndex,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nIndexCount,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nStartInstance,			FIELD_INTEGER ),
	DEFINE_FIELD( m_nInstanceCount,			FIELD_INTEGER ),
	DEFINE_FIELD( m_64Bits,					FIELD_INTEGER64 ),	// TODO: Add int64 to datamap.h
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// BVHNode related
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( BVHNodeHeader_t )
	DEFINE_FIELD( m_nID,					FIELD_INTEGER ),
	DEFINE_FIELD( m_Flags,					FIELD_INTEGER ),
	DEFINE_FIELD( m_nParent,				FIELD_INTEGER ),
	DEFINE_EMBEDDED( m_Origin ),
	DEFINE_EMBEDDED( m_Bounds ),
	DEFINE_FIELD( m_flMinimumDistance,		FIELD_FLOAT ),
	DEFINE_FIELD( m_nChildren,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nResources,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nDrawCalls,				FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// World related
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( BVHBuilderParams_t )
	DEFINE_FIELD( m_nSizeBytesPerVoxel,		FIELD_INTEGER ),	
	DEFINE_FIELD( m_flMinDrawVolumeSize,	FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinDistToCamera,		FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinAtlasDist,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinSimplifiedDist,	FIELD_FLOAT ),
	DEFINE_FIELD( m_flHorzFOV,				FIELD_FLOAT ),
	DEFINE_FIELD( m_flHalfScreenWidth,		FIELD_FLOAT ),
	DEFINE_FIELD( m_nAtlasTextureSizeX,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nAtlasTextureSizeY,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nUniqueTextureSizeX,	FIELD_INTEGER ),
	DEFINE_FIELD( m_nUniqueTextureSizeY,	FIELD_INTEGER ),
	DEFINE_FIELD( m_nCompressedAtlasSize,	FIELD_INTEGER ),
	DEFINE_FIELD( m_flGutterSize,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flUVMapThreshold,		FIELD_FLOAT ),
	DEFINE_FIELD( m_vWorldUnitsPerTile,		FIELD_VECTOR ),
	DEFINE_FIELD( m_nMaxTexScaleSlots,		FIELD_INTEGER ),
	DEFINE_FIELD( m_bWrapInAtlas,			FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_padding,				FIELD_CHARACTER, 3 ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// File header
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( WorldFileHeader_t )
	DEFINE_FIELD( m_nFileVersion,			FIELD_INTEGER ),
	DEFINE_FIELD( m_vWorldUnitsPerTile,		FIELD_VECTOR ),
	DEFINE_FIELD( m_nChunks,				FIELD_INTEGER ),
	DEFINE_EMBEDDED( m_BuilderParams ),
END_BYTESWAP_DATADESC()

//--------------------------------------------------------------------------------------
// Known chunk headers
//--------------------------------------------------------------------------------------
BEGIN_BYTESWAP_DATADESC( HierarchyChunkHeader_t )
	DEFINE_FIELD( m_nNodes,					FIELD_INTEGER ),
	DEFINE_FIELD( m_nMaxNodeSizeBytes,		FIELD_INTEGER ),
	DEFINE_FIELD( m_nAvgNodeSizeBytes,		FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( EntityChunkHeader_t )
	DEFINE_FIELD( m_nEntities,				FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( VisibilityChunkHeader_t )
	DEFINE_FIELD( m_nNodes,					FIELD_INTEGER ),
	DEFINE_FIELD( m_nDWORDS,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nX,						FIELD_INTEGER ),
	DEFINE_FIELD( m_nY,						FIELD_INTEGER ),
	DEFINE_FIELD( m_nZ,						FIELD_INTEGER ),
	DEFINE_FIELD( m_vCellSize,				FIELD_VECTOR ),
	DEFINE_FIELD( m_vStart,					FIELD_VECTOR ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( RenderInputLayoutFieldProxy_t )
	DEFINE_ARRAY( m_pSemanticName,			FIELD_CHARACTER, RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE ),
	DEFINE_FIELD( m_nSemanticIndex,			FIELD_INTEGER ),
	DEFINE_FIELD( m_Format,					FIELD_INTEGER ),
	DEFINE_FIELD( m_nOffset,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nSlot,					FIELD_INTEGER ),
	DEFINE_FIELD( m_nSlotType,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nInstanceStepRate,		FIELD_INTEGER ),
END_BYTESWAP_DATADESC()