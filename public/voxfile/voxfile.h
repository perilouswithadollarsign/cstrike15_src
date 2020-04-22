//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VOXFILE_H
#define VOXFILE_H
#ifdef _WIN32
#pragma once
#endif

#pragma pack(1)
const uint16 DVOX_CHILD_NOT_PRESENT = 0xFFFF;
struct dvoxtreenode_t
{
	uint16 childNodes[8];
	uint32 voxelIndex;
	uint32 treeLevel;
};

struct dvoxelvert_t
{
	int16 pos[3];
	uint32 normal;
};

struct dvoxel_t
{
	uint32	fileOffset;
	uint32	fileSize;
	uint32	vertexCount;
	uint32	indexCount;
	float	vertexScale;
	Vector	origin;			// quantized grid origin
	Vector	localMins;		// origin relative bbox
	Vector	localMaxs;		// ...
};

struct dvoxfilechunk_t
{
	uint32	identFourCC;
	uint32	version;
	uint32	fileOffset;
	uint32	fileSize;
};

// 1KB header
struct dvoxelfileheader_t
{
	uint32		identFourCC;
	uint32		voxelCount;				// number of voxels in the file including LODs etc
	uint32		voxTreeNodeCount;		// number of nodes in the tree
	uint32		voxTreeNodeRefCount;	// number of references to nodes

	uint32		voxTreeTopLevelNodeCount; // number of top level nodes in the tree
	uint32		chunkCount;
	uint32		pad0;
	uint32		pad1;

	dvoxfilechunk_t chunks[62];
};
#pragma pack()

#define VOXEL_FILEID		MAKEID('V','M','A','P')

#define VOX_CHUNK_VOXELS	MAKEID('V','V','O','X')
#define VOX_CHUNK_VOXELTREE	MAKEID('T','R','E','E')
#define VOX_CHUNK_VOXELGRID	MAKEID('G','R','I','D')
#define VOX_CHUNK_ENTITIES	MAKEID('E','N','T','S')


#endif // VOXFILE_H
