//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#ifdef _WIN32
#pragma once
#endif

#include "studio.h"

#define MAX_NUM_BONES_PER_STRIP 512

#define OPTIMIZED_MODEL_FILE_VERSION 7

extern bool g_bDumpGLViewFiles;

struct s_bodypart_t;

namespace OptimizedModel
{

#pragma pack(1)

struct BoneStateChangeHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int hardwareID;
	int newBoneID;
};

struct Vertex_t
{
	DECLARE_BYTESWAP_DATADESC();
	// these index into the mesh's vert[origMeshVertID]'s bones
	unsigned char boneWeightIndex[MAX_NUM_BONES_PER_VERT];
	unsigned char numBones;

	unsigned short origMeshVertID;

	// for sw skinned verts, these are indices into the global list of bones
	// for hw skinned verts, these are hardware bone indices
	byte boneID[MAX_NUM_BONES_PER_VERT];
};

// We don't do actual strips anymore, only triangle lists and subd quad lists
enum StripHeaderFlags_t
{
	STRIP_IS_TRILIST		= 0x01,
	STRIP_IS_QUADLIST_REG	= 0x02,		// Regular sub-d quads
	STRIP_IS_QUADLIST_EXTRA = 0x04		// Extraordinary sub-d quads
};

// A strip is a piece of a stripgroup that is divided by bones 
struct StripHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int numIndices;				// indexOffset offsets into the mesh's index array
	int indexOffset;

	int numVerts;				// vertexOffset offsets into the mesh's vert array
	int vertOffset;

	// Use this to enable/disable skinning.
	// May decide (in optimize.cpp) to put all with 1 bone in a different strip
	// than those that need skinning.
	short numBones;
	
	unsigned char flags;
	
	int numBoneStateChanges;
	int boneStateChangeOffset;
	inline BoneStateChangeHeader_t *pBoneStateChange( int i ) const
	{
		return (BoneStateChangeHeader_t *)(((byte *)this) + boneStateChangeOffset) + i;
	};

	// These go last on purpose!
	int numTopologyIndices;
	int topologyOffset;
};

enum StripGroupFlags_t
{
	STRIPGROUP_IS_HWSKINNED		 = 0x02,
	STRIPGROUP_IS_DELTA_FLEXED	 = 0x04,
	STRIPGROUP_SUPPRESS_HW_MORPH = 0x08,	// NOTE: This is a temporary flag used at run time.
};

// a locking group
// a single vertex buffer
// a single index buffer
struct StripGroupHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	// These are the arrays of all verts and indices for this mesh.  strips index into this.
	int numVerts;
	int vertOffset;
	inline Vertex_t *pVertex( int i ) const 
	{ 
		return (Vertex_t *)(((byte *)this) + vertOffset) + i; 
	};

	int numIndices;
	int indexOffset;
	inline unsigned short *pIndex( int i ) const 
	{ 
		return (unsigned short *)(((byte *)this) + indexOffset) + i; 
	};

	int numStrips;
	int stripOffset;
	inline StripHeader_t *pStrip( int i ) const 
	{ 
		return (StripHeader_t *)(((byte *)this) + stripOffset) + i; 
	};

	unsigned char flags;

	int numTopologyIndices;
	int topologyOffset;
	inline unsigned short *pTopologyIndex( int i ) const 
	{ 
		return (unsigned short *)(((byte *)this) + topologyOffset) + i; 
	};
};

enum MeshFlags_t { 
	// these are both material properties, and a mesh has a single material.
	MESH_IS_TEETH	= 0x01, 
	MESH_IS_EYES	= 0x02
};

// a collection of locking groups:
// up to 4:
// non-flexed, hardware skinned
// flexed, hardware skinned
// non-flexed, software skinned
// flexed, software skinned
//
// A mesh has a material associated with it.
struct MeshHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int numStripGroups;
	int stripGroupHeaderOffset;
	inline StripGroupHeader_t *pStripGroup( int i ) const 
	{ 
		StripGroupHeader_t *pDebug = (StripGroupHeader_t *)(((byte *)this) + stripGroupHeaderOffset) + i; 
		return pDebug;
	};
	unsigned char flags;
};

struct ModelLODHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int numMeshes;
	int meshOffset;
	float switchPoint;
	inline MeshHeader_t *pMesh( int i ) const 
	{ 
		MeshHeader_t *pDebug = (MeshHeader_t *)(((byte *)this) + meshOffset) + i; 
		return pDebug;
	};
};

// This maps one to one with models in the mdl file.
// There are a bunch of model LODs stored inside potentially due to the qc $lod command
struct ModelHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int numLODs; // garymcthack - this is also specified in FileHeader_t
	int lodOffset;
	inline ModelLODHeader_t *pLOD( int i ) const 
	{ 
		ModelLODHeader_t *pDebug = ( ModelLODHeader_t *)(((byte *)this) + lodOffset) + i; 
		return pDebug;
	};
};

struct BodyPartHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int numModels;
	int modelOffset;
	inline ModelHeader_t *pModel( int i ) const 
	{ 
		ModelHeader_t *pDebug = (ModelHeader_t *)(((byte *)this) + modelOffset) + i;
		return pDebug;
	};
};

struct MaterialReplacementHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	short materialID;
	int replacementMaterialNameOffset;
	inline const char *pMaterialReplacementName( void )
	{
		const char *pDebug = (const char *)(((byte *)this) + replacementMaterialNameOffset); 
		return pDebug;
	}
};

struct MaterialReplacementListHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	int numReplacements;
	int replacementOffset;
	inline MaterialReplacementHeader_t *pMaterialReplacement( int i ) const
	{
		MaterialReplacementHeader_t *pDebug = ( MaterialReplacementHeader_t *)(((byte *)this) + replacementOffset) + i; 
		return pDebug;
	}
};

struct FileHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	// file version as defined by OPTIMIZED_MODEL_FILE_VERSION
	int version;

	// hardware params that affect how the model is to be optimized.
	int vertCacheSize;
	unsigned short maxBonesPerStrip;
	unsigned short maxBonesPerFace;
	int maxBonesPerVert;

	// must match checkSum in the .mdl
	int checkSum;
	
	int numLODs; // garymcthack - this is also specified in ModelHeader_t and should match

	// one of these for each LOD
	int materialReplacementListOffset;
	MaterialReplacementListHeader_t *pMaterialReplacementList( int lodID ) const
	{ 
		MaterialReplacementListHeader_t *pDebug = 
			(MaterialReplacementListHeader_t *)(((byte *)this) + materialReplacementListOffset) + lodID;
		return pDebug;
	}

	int numBodyParts;
	int bodyPartOffset;
	inline BodyPartHeader_t *pBodyPart( int i ) const 
	{
		BodyPartHeader_t *pDebug = (BodyPartHeader_t *)(((byte *)this) + bodyPartOffset) + i;
		return pDebug;
	};	
};

#pragma pack()

void WriteOptimizedFiles( studiohdr_t *phdr, s_bodypart_t *pSrcBodyParts );

}; // namespace OptimizedModel

#endif // OPTIMIZE_H
