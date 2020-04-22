//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Class & functions to parse and represent a VMF file.
//
//===============================================================================

#ifndef SIMPLEMAPFILE_H
#define SIMPLEMAPFILE_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "chunkfile.h"
#include "stringpool.h"

#include "vbspmathutil.h"

class CSimpleMapFile;
class GameData;

DECLARE_LOGGING_CHANNEL( LOG_VBSP2 );

//-----------------------------------------------------------------------------
// Flags that modify a brush volume or brush side surface
//-----------------------------------------------------------------------------
typedef int MapBrushContentsFlags_t;

//-----------------------------------------------------------------------------
// Flags that modify a brush side surface
//-----------------------------------------------------------------------------
typedef short MapBrushSideSurfaceFlags_t;

//-----------------------------------------------------------------------------
// A single entity from a VMF file.
//-----------------------------------------------------------------------------
struct MapEntity_t
{
	// World-space location of the object.
	Vector m_vOrigin;

	// Index & count of brushes contained within this entity. These are stored in the map's brush array.
	// Brushes in an entity have contiguous indices in the range: [m_nFirstBrushIndex, m_nFirstBrushIndex + m_nNumBrushes).
	int m_nFirstBrushIndex;
	int m_nNumBrushes;

	// Index & count of key-value pairs (entity properties) contained within this entity. These are stored in the map's key-value pair array.
	// Key-value pairs in an entity have contiguous indices in the range: [m_nFirstKVPairIndex, m_nFirstKVPairIndex + m_nNumKVPairs).
	int m_nFirstKVPairIndex;
	int m_nNumKVPairs;
};

//-----------------------------------------------------------------------------
// A property of an entity from a VMF file.
//-----------------------------------------------------------------------------
struct MapEntityKeyValuePair_t
{
	// These point into the string pool owned by the map object, so no memory management is necessary.
	const char *m_pKey;
	const char *m_pValue;

	// True if this key value pair came from the "connections" section of an entity, 
	// false if it came from an ordinary entity key.
	bool m_bIsConnection;
};

//-----------------------------------------------------------------------------
// A CSG brush (convex solid) from a VMF file.
//-----------------------------------------------------------------------------
struct MapBrush_t
{
	// Contents of the brush, based on the properties of its sides.
	MapBrushContentsFlags_t m_ContentsFlags;

	// Index & count of brush sides.  These are stored in the map's brush sides array.
	// Brush sides have contiguous indices in the range: [m_nFirstSideIndex, m_nFirstSideIndex + m_nNumSides).
	int m_nFirstSideIndex;	
	int m_nNumSides;

	// AABB of the brush
	Vector m_vMinBounds, m_vMaxBounds;
};

static const int INVALID_DISPLACEMENT_INDEX = -1;

//-----------------------------------------------------------------------------
// A single side of a CSG brush from a VMF file.
//-----------------------------------------------------------------------------
struct MapBrushSide_t
{
	// Contents of the brush side, based on the material properties.
	MapBrushContentsFlags_t m_ContentsFlags;
	// Surface flags of the brush side, based on the material properties.
	MapBrushSideSurfaceFlags_t m_SurfaceFlags;
	// The index of the plane coincident with this brush side.  The index is in the map's plane hash.
	int m_nPlaneIndex;
	// The index of the texture info struct for this brush side, describing the material applied to this side.
	// The index is into the map's texture info array.
	int m_nTextureInfoIndex;
	// The polygonal face of this brush side.
	Polygon_t m_Polygon;
	// The index of the displacement associated with this side, or INVALID_DISPLACEMENT_INDEX if
	// this side is not a displacement surface.
	int m_nDisplacementIndex;
};

//-----------------------------------------------------------------------------
// A displacement surface, specified on a particular CSG brush side.
//-----------------------------------------------------------------------------
class CMapDisplacement
{
public:
	CMapDisplacement();

	bool AreVerticesAllocated() const { return m_Vertices.Count() > 0 && m_nPower > 0; }

	// NOTE: If new fields are added to this class, you must update MoveFrom()!

	// Power-of-2 number of subdivisions.
	// MIN_MAP_DISP_POWER <= m_nPower <= MAX_MAP_DISP_POWER
	int m_nPower;
	// Map brush side corresponding to this displacement surface.  The brush side is "orphaned"
	// in that no brushes will point to it (since the original brush is removed from the world).
	int m_nOriginalBrushSide;
	// SURF_* flags, defined in builddisp.h
	int m_nFlags;
	// Contents flags taken from the original brush. 
	// The value is stored here, since the original brush gets removed from the world.
	MapBrushContentsFlags_t m_ContentsFlags;
	Vector m_vStartPosition;

	struct Vertex_t
	{
		float m_flAlpha;
		float m_flDistance;
		Vector m_vNormal;
		Vector m_vOffset;
		Vector4D m_vMultiBlend;
		Vector m_vMultiBlendColors[MAX_MULTIBLEND_CHANNELS];
	};

	CUtlVector< Vertex_t > m_Vertices;

	// DISPTRI_* flags, defined in bspfile.h (one per triangle).
	CUtlVector< unsigned short > m_TriangleTags;

	// Copies values and takes ownership of owned data from the other displacement object
	// (this is a destructive, but efficient, operation used to migrate a displacement
	// from one map object to another).
	// NOTE: If you add new fields to CMapDisplacement, you must update this function!
	void MoveFrom( CMapDisplacement *pOther );

private:
	// Disallow value semantics
	CMapDisplacement( const CMapDisplacement &other );
	CMapDisplacement &operator=( const CMapDisplacement &other );
};

static const int MAX_TEXTURE_NAME_LENGTH = 128;

//-----------------------------------------------------------------------------
// Texture properties for a brush side, as loaded from a VMF file.
//-----------------------------------------------------------------------------
struct MapBrushTexture_t
{
	Vector m_vUAxis, m_vVAxis;
	float m_flShift[2];
	float m_flTextureWorldUnitsPerTexel[2];
	float m_flLightmapWorldUnitsPerLuxel;
	char m_MaterialName[MAX_TEXTURE_NAME_LENGTH];
	MapBrushSideSurfaceFlags_t m_SurfaceFlags;
};

//-----------------------------------------------------------------------------
// Sentinel texture info index for a side of a solid BSP node.
//-----------------------------------------------------------------------------
static const int NODE_TEXTURE_INFO_INDEX = -1;

//-----------------------------------------------------------------------------
// Planar mapping of texture data to a particular surface.
//-----------------------------------------------------------------------------
struct MapTextureInfo_t
{
	// The U and V vectors (w-component contains the offset) that define
	// a homogeneous planar mapping from 3D (world) space to 2D (texture) space.
	float m_flTextureVectors[2][4];
	float m_flLightmapVectors[2][4];
	// Index into the map's texture data array of the structure which describes
	// the material being mapped to this surface.
	int m_nTextureDataIndex;
	MapBrushSideSurfaceFlags_t m_SurfaceFlags;
};

//-----------------------------------------------------------------------------
// Basic material information for a surface.
//-----------------------------------------------------------------------------
struct MapTextureData_t
{
	Vector m_vReflectivity;	
	// Width and height of the base texture referenced by the material
	int m_nWidth, m_nHeight;
	// Name of the material (e.g. DEV/GRAYGRID) being mapped
	char m_MaterialName[MAX_TEXTURE_NAME_LENGTH];
};

//-----------------------------------------------------------------------------
// Finds a key-value pair with the specified key in a given array of 
// key-value pairs, or NULL if none is found
//-----------------------------------------------------------------------------
const MapEntityKeyValuePair_t *FindPair( const char *pKeyName, const MapEntityKeyValuePair_t *pPairs, int nNumPairs );
MapEntityKeyValuePair_t *FindPair( const char *pKeyName, MapEntityKeyValuePair_t *pPairs, int nNumPairs );

//-----------------------------------------------------------------------------
// Gets the value associated with the given pair or "" if the pair
// or value string is NULL (safe to chain with FindPair)
//-----------------------------------------------------------------------------
const char *GetPairValue( const MapEntityKeyValuePair_t *pPair );

//-----------------------------------------------------------------------------
// A class which represents a VMF (map) file in memory
// or value string is NULL (safe to chain with FindPair)
//-----------------------------------------------------------------------------
class CSimpleMapFile
{
public:
	//-----------------------------------------------------------------------------
	// Gets the minimum and maximum bound of all world brushes in this map file
	//-----------------------------------------------------------------------------
	Vector GetMinBounds() const { return m_vMinBounds; }
	Vector GetMaxBounds() const { return m_vMaxBounds; }

	int GetMapRevision() const { return m_nMapRevision; }

	const MapEntity_t *GetEntities() const { return &m_Entities[0]; }
	MapEntity_t *GetEntities() { return &m_Entities[0]; }
	int GetEntityCount() const { return m_Entities.Count(); }

	//-----------------------------------------------------------------------------
	// Gets the world entity, which is the 0th entity in the file
	//-----------------------------------------------------------------------------
	const MapEntity_t *GetWorldEntity() const { return GetEntities(); }
	MapEntity_t *GetWorldEntity() { return GetEntities(); }

	const MapBrush_t *GetBrushes() const { return &m_Brushes[0]; }
	MapBrush_t *GetBrushes() { return &m_Brushes[0]; }
	int GetBrushCount() const { return m_Brushes.Count(); }

	const MapBrushSide_t *GetBrushSides() const { return &m_BrushSides[0]; }
	MapBrushSide_t *GetBrushSides() { return &m_BrushSides[0]; }
	int GetBrushSideCount() const { return m_BrushSides.Count(); }

	const CMapDisplacement *GetDisplacements() const { return &m_Displacements[0]; }
	CMapDisplacement *GetDisplacements() { return &m_Displacements[0]; }
	int GetDisplacementCount() const { return m_Displacements.Count(); }

	const MapTextureInfo_t *GetTextureInfos() const { return &m_TextureInfos[0]; }
	MapTextureInfo_t *GetTextureInfos() { return &m_TextureInfos[0]; }
	int GetTextureInfoCount() const { return m_TextureInfos.Count(); }

	const MapTextureData_t *GetTextureData() const { return &m_TextureData[0]; }
	MapTextureData_t *GetTextureData() { return &m_TextureData[0]; }
	int GetTextureDataCount() const { return m_TextureData.Count(); }

	const MapEntityKeyValuePair_t *GetKeyValuePairs() const { return &m_KeyValuePairs[0]; }
	MapEntityKeyValuePair_t *GetKeyValuePairs() { return &m_KeyValuePairs[0]; }
	int GetKeyValuePairCount() const { return m_KeyValuePairs.Count(); }

	const CPlaneHash *GetPlaneHash() const { return &m_PlaneHash; }

	const char *GetFilename() const { return m_pFilename; }

	//-----------------------------------------------------------------------------
	// Adds a func_instance entity to the map file.
	//-----------------------------------------------------------------------------
	void AddFuncInstance( const char *pFilename, QAngle angles, Vector vOrigin, MapEntityKeyValuePair_t *pExtraKeyValues = NULL, int nExtraKeyValues = 0 );

	int FindEntity( const char *pClassName, const char *pKeyName, const char *pValue, int nStartingIndex = 0 );
	void RemoveEntity( int nEntityIndex );
	
	//-----------------------------------------------------------------------------
	// Flags passed to ResolveInstances() to control behavior.
	//-----------------------------------------------------------------------------
	enum ResolveInstanceFlags_t
	{
		NO_FLAGS = 0x0,
		CONVERT_STRUCTURAL_TO_DETAIL = 0x1, // convert all solids in world entities to func_detail 
	};

	//-----------------------------------------------------------------------------
	// Callback invoked after loading each instance.
	// The callback is given the originally provided context value, the loaded 
	// instance map, and the key-value pairs from the parent map's 
	// func_instance entity chunk.
	//-----------------------------------------------------------------------------
	typedef void ( *PostLoadInstanceHandler_t )( void *pContext, CSimpleMapFile *pInstanceMapFile, MapEntityKeyValuePair_t *pFuncInstanceKeyValuePairs, int nNumKeyValuePairs );

	//-----------------------------------------------------------------------------
	// Recursively loads all contained func_instance entities and merges
	// them with the current map in breadth-first recursive order.
	//-----------------------------------------------------------------------------
	bool ResolveInstances( ResolveInstanceFlags_t instanceFlags = NO_FLAGS, PostLoadInstanceHandler_t pPostLoadInstanceHandler = NULL, void *pHandlerContext = NULL );

	//-----------------------------------------------------------------------------
	// Loads a VMF (map) file from disk and constructs a new CSimpleMapFile object.
	// The caller must delete the object when done.
	//-----------------------------------------------------------------------------
	static void LoadFromFile( IFileSystem *pFileSystem, const char *pVMFFilename, CSimpleMapFile **ppNewMapFile, ResolveInstanceFlags_t instanceFlags = NO_FLAGS );

private:
	CSimpleMapFile( IFileSystem *pFileSystem, const char *pVMFFilename );

	MapEntity_t *AllocateNewEntity();	
	MapBrush_t *AllocateNewBrush();	
	MapBrushSide_t *AllocateNewBrushSide();	
	CMapDisplacement *AllocateNewDisplacement();	
	MapBrushTexture_t *AllocateNewBrushTexture();
	MapTextureData_t *AllocateNewTextureData();
	MapEntityKeyValuePair_t *AllocateNewKeyValuePair();

	// Some entity types are simply ignored
	void DeallocateLastEntity() { m_Entities.RemoveMultipleFromTail( 1 ); }

	// Some brushes are loaded and then discarded (e.g. brushes with displacement)
	void DeallocateLastBrush() { m_Brushes.RemoveMultipleFromTail( 1 ); }

	void ReportParseError( const char *pErrorString );
	
	void MakeBrushPolygons( MapBrush_t *pBrush );

	void MoveEntityBrushesToWorld( int nEntityIndex );

	int GetTextureInfoForBrushTexture( MapBrushTexture_t *pBrushTexture, const Vector &vRelativeOrigin );
	int FindOrCreateTextureData( const char *pTextureName );
	int FindOrCreateTextureInfo( const MapTextureInfo_t &textureInfo );
	MapBrushContentsFlags_t	ComputeBrushContents( MapBrush_t *b );

	// Functions used to merge instance data (recursively) with the primary map
	void MergeInstance( int nEntityIndex, CSimpleMapFile *pInstanceMap, GameData *pGameData );
	void PreLoadInstances( GameData *pGD );
	void MergeBrushes( int nEntityIndex, const CSimpleMapFile *pInstanceMap, const Vector &vOrigin, const QAngle &orientation, const matrix3x4_t &transform );
	void MergeBrushSides( int nEntityIndex, CSimpleMapFile *pInstanceMap, const Vector &vOrigin, const QAngle &orientation, const matrix3x4_t &transform );
	void MergeEntities( int nEntityIndex, const CSimpleMapFile *pInstanceMap, const Vector &vOrigin, const QAngle &orientation, const matrix3x4_t &transform, GameData *pGameData );
	void FixupInstanceKeyValuePair( MapEntityKeyValuePair_t *pNewKeyValuePair, const char *pOriginalValue, const MapEntityKeyValuePair_t *pInstancePairs, int nNumPairs );

	// The following static functions are passed as callbacks to the chunk file reader 
	// when certain chunks and key blocks are encountered in the VMF file.
	static ChunkFileResult_t EntityChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t SolidChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t SideChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DispInfoChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t ConnectionsChunkHandler( CChunkFile *pFile, void *pData );

	static ChunkFileResult_t EntityKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t SolidKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t SideKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementKeyHandler( const char *pKey, const char *pValue, void *pData );

	// These are shim functions which simply call into their corresponding displacement key handlers
	static ChunkFileResult_t DisplacementNormalsChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementDistancesChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementOffsetsChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementAlphasChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementTriangleTagsChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendChunkHandler( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendColor0( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendColor1( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendColor2( CChunkFile *pFile, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendColor3( CChunkFile *pFile, void *pData );

	// These read the actual key data associated with each displacement attribute
	static ChunkFileResult_t DisplacementNormalsKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementDistancesKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementOffsetsKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementAlphasKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementTriangleTagsKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendKeyHandler( const char *pKey, const char *pValue, void *pData );
	static ChunkFileResult_t DisplacementMultiBlendColorKeyHandler( const char *pKey, const char *pValue, void *pData );

	static ChunkFileResult_t ConnectionsKeyHandler( const char *pKey, const char *pValue, void *pData );

	struct MaterialInfo_t
	{
		char m_Name[MAX_TEXTURE_NAME_LENGTH];
		MapBrushContentsFlags_t m_ContentsFlags;
		MapBrushSideSurfaceFlags_t m_SurfaceFlags;
	};
	MaterialInfo_t* FindMaterialInfo( const char *pName );

	IFileSystem *m_pFileSystem;

	// This is needed so instances can be resolved.  If we're loading map files from memory buffers
	// or other non-file sources, we'll need an "include interface" (like D3DX/HLSL's include resolution system).
	const char *m_pFilename;

	CPlaneHash m_PlaneHash;
	CUtlVector< MapEntity_t > m_Entities;
	CUtlVector< MapBrush_t > m_Brushes;
	CUtlVector< CMapDisplacement > m_Displacements;
	// These two arrays (m_BrushSides & m_BrushTextures) should always be the same size as there is a 1:1 correspondence.
	CUtlVector< MapBrushSide_t > m_BrushSides;
	CUtlVector< MapBrushTexture_t > m_BrushTextures;
	CUtlVector< MapTextureInfo_t > m_TextureInfos;
	CUtlVector< MapTextureData_t > m_TextureData;

	// Number of instances loaded (recursively).
	// This value is only used on the root-level map. It is used to generate unique names for instanced entities.
	int m_nInstanceCount;
	
	// This value is only used on instance maps to control loading and processing.
	ResolveInstanceFlags_t m_InstanceFlags;

	// Used to store entity property data
	CUtlVector< MapEntityKeyValuePair_t > m_KeyValuePairs;
	CStringPool m_KeyValueStringPool;

	Vector m_vMinBounds, m_vMaxBounds;

	int m_nMapRevision;
	int m_nHighestID;

	// Used to cache results of FindMaterialInfo, which gets information about materials applied to brush sides.
	CUtlVector< MaterialInfo_t > m_MaterialInfos;
};

#endif // SIMPLEMAPFILE_H