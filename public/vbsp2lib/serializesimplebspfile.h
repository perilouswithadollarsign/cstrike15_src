//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Class to represent and read/write a BSP file.
//
//===============================================================================

#ifndef SERIALIZESIMPLEBSPFILE_H
#define SERIALIZESIMPLEBSPFILE_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "bspfile.h"
#include "gamebspfile.h"
#include "builddisp.h"
#include "vbspmathutil.h"
#include "simplemapfile.h"

class CSimpleBSPFile;
class CBSPNode;
class CBSPFace;

class CPhysCollisionEntry;
class IPhysicsCollision;
class IPhysicsSurfaceProps;

class CUtlBuffer;

//-----------------------------------------------------------------------------
// Serializes a BSP file to a provided buffer.
//-----------------------------------------------------------------------------
void SaveToFile( CUtlBuffer *pOutputBuffer, const CSimpleBSPFile *pBSPFile );

//-----------------------------------------------------------------------------
// Class to build and manipulate a BSP file in its on-disk representation.
//
// This is a lower-level representation of a BSP than CSimpleBSPFile.
//
// Can be used in one of several modes:
//
// 1) Serializing a .bsp file - 
//
//    a) Given the expanded, in-memory representation
//    of a compiled map (CSimpleBSPFile), this class will build
//    intermediate representations of various structures where needed and
//    serialize the data to a .bsp file.  It can be operated in a "memory
//    efficient" mode where lumps are written out and freed as soon as 
//    possible.
//
//    b) This class can also serialize a BSP file that was constructed
//    in memory or previously deserialized.
//
// 2) Deserializing a .bsp file - the recognized lump types will be loaded
//    and stored in memory, at which point they can be manipulated
//    as needed.
//    WARNING: only recognized lumps will be deserialized; this file 
//    has incomplete support for many lump types.
//
//-----------------------------------------------------------------------------
class CMemoryBSPFile
{
public:
	CMemoryBSPFile();

	//-----------------------------------------------------------------------------
	// Grabs data from the BSP class and serializes it to the output buffer.
	// The output buffer must be at the start of an empty output stream.
	//-----------------------------------------------------------------------------
	void ProcessAndSerialize( CUtlBuffer *pOutputBuffer, const CSimpleBSPFile *pSimpleBSPFile );

	//-----------------------------------------------------------------------------
	// Writes data to a .bsp file buffer.
	//-----------------------------------------------------------------------------
	void Serialize( CUtlBuffer *pOutputBuffer );

	//-----------------------------------------------------------------------------
	// Reads .bsp data from a buffer into lump-specific arrays.
	// Class can then be modified and re-saved to disk with Serialize().
	//-----------------------------------------------------------------------------
	void Deserialize( CUtlBuffer *pInputBuffer );

	bool TryGetFaceVertex( int nFace, int nVertexIndex, Vector *pPosition ) const;
	int GetLeafIndexFromPoint( int nNodeIndex, const Vector &vPosition ) const;


private:
	bool IsHeaderValid() { return m_FileHeader.ident == IDBSPHEADER; }

	// Begin & End are used when you want to manually Put() data into the buffer
	void BeginWriteLump( int nLump, int nByteLength, int nVersion = 0 );
	void EndWriteLump();

	// Writes a whole lump (do not call Begin/EndWriteLump)
	void WriteLump( int nLump, const void *pData, int nByteLength, int nVersion = 0 );

	template< typename T >
	void WriteLump( int nLump, const CUtlVector< T > &data, int nVersion = 0 );

	// Begin & End are used when you want to manually Get() data from the buffer
	int BeginReadLump( int nLump ); // returns size of the lump
	void EndReadLump();

	// Read a lump into an array (do not call Begin/EndReadLump)
	template< typename T >
	void ReadLump( int nLump, CUtlVector< T > *pVector );

	void BuildTexInfo();
	void WriteTexInfo( bool bPurgeWhenComplete );
	void BuildTexData();
	int FindOrAddString( const char *pString );
	void WriteTexData( bool bPurgeWhenComplete );

	void BuildModelData();
	void WriteModelData( bool bPurgeWhenComplete );

	void WritePortalFaces( const CBSPNode *pNode );

	// There are 3 types of faces that can be emitted:
	// 1) Portal faces, emitted all at once when a model is written
	// 2) Detail faces, emitted after portal faces (also all at once when a model is written)
	// 3) Displacement faces, emitted only for the world model (model #0), written all at once with the world model
	void EmitFace( const CBSPFace *pBSPFace, bool bOnNode );

	void BuildBSPTreeData();
	void WriteBSPTreeData( bool bPurgeWhenComplete );

	int32 EmitLeaf( const CBSPNode *pNode );
	int32 EmitNode( const CBSPNode *pNode );

	void BuildBrushes();
	void WriteBrushes( bool bPurgeWhenComplete );

	void BuildPlanes();
	void WritePlanes( bool bPurgeWhenComplete );

	void WriteModels( bool bPurgeWhenComplete );

	void WriteDummyAreasAndAreaPortals();

	void BuildGameLumpData();
	void EmitStaticProp( const MapEntity_t *pEntity, CUtlVector< StaticPropLump_t > *pStaticPropLump, CUtlVector< StaticPropLeafLump_t > *pStaticPropLeafLump, CUtlVector< StaticPropDictLump_t > *pStaticPropDictionaryLump );
	int AddStaticPropDictionaryEntry( const char *pModelName, CUtlVector< StaticPropDictLump_t > *pStaticPropDictionaryLump );
	int AddStaticPropLeaves( const Vector &vOrigin, int nNodeIndex, CUtlVector< StaticPropLeafLump_t > *pStaticPropLeafLump );
	void WriteGameLumpData( bool bPurgeWhenComplete );

	void BuildEntityData();
	void StripTrailingJunkCharacters( char *pString );
	void WriteEntityData( bool bPurgeWhenComplete );

	void BuildVisibilityData();
	void WriteVisibilityData( bool bPurgeWhenComplete );

	void BuildDisplacements();
	void WriteDisplacements( bool bPurgeWhenComplete );

	void BuildPhysicsCollisionData();
	CPhysCollisionEntry * CreateWorldPhysicsModels( 
		IPhysicsCollision *pPhysicsCollision,
		CUtlVector< int > *pWorldPropertyRemapList, 
		const int *pSurfacePropertyList, 
		const dmodel_t *pModel,
		MapBrushContentsFlags_t contentsMask );
	CPhysCollisionEntry * CreatePhysicsModel( 
		IPhysicsCollision *pPhysicsCollision,
		IPhysicsSurfaceProps *pPhysicsProperties,
		const int *pSurfacePropertyList,
		const dmodel_t *pModel );
	void BuildDisplacementVirtualMesh( IPhysicsCollision *pPhysicsCollision );
	const MapBrushSide_t *FindClosestBrushSide( int nBrushIndex, const Vector &vNormal );
	int RemapWorldMaterial( CUtlVector< int > *pWorldPropertyRemapList, int nSurfacePropertyIndex );

	void WritePhysicsCollisionData( bool bPurgeWhenComplete );

	void WriteLightingData( bool bPurgeWhenComplete );

	CUtlBuffer *m_pSerialBuffer;

	const CSimpleBSPFile *m_pSimpleBSPFile;
	const CSimpleMapFile *m_pMapFile;

	CPlaneHash m_PlaneHash;

	BSPHeader_t m_FileHeader;

public:
	// The following arrays map 1:1 with on-disk BSP data.
	CUtlVector< texinfo_t > m_TexInfoList;
	CUtlVector< dtexdata_t > m_TexDataList;

	CUtlVector< char > m_TexStringData;
	CUtlVector< int32 > m_TexStringIndices;

	CUtlVector< dmodel_t > m_ModelList;
	CVertexHash m_VertexHash;
	CUtlVector< dedge_t > m_EdgeList;
	CUtlVector< int32 > m_SurfEdgeList;
	CUtlVector< dface_t > m_FaceList;
	CUtlVector< Vector > m_VertexNormalList;
	CUtlVector< uint16 > m_VertexNormalIndexList;

	CUtlVector< dnode_t > m_NodeList;
	CUtlVector< dleaf_t > m_LeafList;
	CUtlVector< uint16 > m_LeafBrushList;
	CUtlVector< uint16 > m_LeafFaceList;

	CUtlVector< dbrush_t > m_BrushList;
	CUtlVector< dbrushside_t > m_BrushSideList;

	CUtlVector< dplane_t > m_Planes;

	CUtlVector< byte > m_GameLumpData;

	CUtlVector< byte > m_EntityData;

	CUtlVector< byte > m_VisibilityData;

	CUtlVector< CCoreDispInfo * > m_DisplacementHelperList;
	CUtlVector< ddispinfo_t > m_DisplacementList;
	CUtlVector< CDispVert > m_DisplacementVertexList;
	CUtlVector< CDispTri > m_DisplacementTriangleList;
	CUtlVector< CDispMultiBlend > m_DisplacementMultiBlendList;

	CUtlVector< byte > m_PhysicsDisplacementData;
	CUtlVector< byte > m_PhysicsCollideData;

	CUtlVector< byte > m_LightingData;
	CUtlVector< dworldlight_t > m_WorldLightsLDR;
	CUtlVector< dworldlight_t > m_WorldLightsHDR;

	// If entityExclusionFlags[N] is set to true, then do not write entity data for entity NF
	CBitVec< MAX_MAP_ENTITIES > m_EntityExclusionFlags;
};

#endif // SERIALIZESIMPLEBSPFILE_H