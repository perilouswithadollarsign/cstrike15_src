//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Classes to aid in transferring lightmap data (luxels) between BSPs.
//
//===============================================================================

#ifndef LIGHTMAPTRANSFER_H
#define LIGHTMAPTRANSFER_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "mathlib/vector.h"
#include "mathlib/intvector3d.h"
#include "bspfile.h"

class CMemoryBSPFile;

//-----------------------------------------------------------------------------
// A class which acts as a spatial hash for lightmap values.
// Multiple source files can be processed into the hash such that every single
// lightmap sample has its own entry in the hash.
// This data can then be copied into the lightmap lump of a given BSP file.
//-----------------------------------------------------------------------------
class CLuxelHash
{
public:
	CLuxelHash( int nHashBucketCount );

	//-----------------------------------------------------------------------------
	// Adds all of the luxels in the source BSP file to a spatial hash.
	// vOffset is the world-space translation to add to all coordinates in the 
	// source BSP file.
	//-----------------------------------------------------------------------------
	void AddSourceBSPFile( const CMemoryBSPFile *pSourceBSPFile, const Vector &vOffset );

	//-----------------------------------------------------------------------------
	// Copies lighting from the luxel hash into the target BSP file.
	// Values which are not found are initialized to black.
	//-----------------------------------------------------------------------------
	void CopyLighting( CMemoryBSPFile *pTargetBSPFile ) const;

private:
	// nDataLength is the number of ColorRGBExp32 (4 bytes each) per luxel. 
	// This is typically 1 for lightmaps without bump light information, or 4 for those with.
	void AddLuxel( const Vector &vPosition, const Vector &vNormal, const ColorRGBExp32 *pLuxelData, int nDataLength );
	bool FindLuxel( const Vector &vPosition, const Vector &vNormal, const ColorRGBExp32 **ppLuxelData, int *pDataLength ) const;

	void CopyFaceLighting( CMemoryBSPFile *pTargetBSPFile, int nFace ) const;
	void CopyWorldLights( CMemoryBSPFile *pTargetBSPFile ) const;
	
	// Hashes the position and returns a hash bucket index in the range [ 0, m_HashEntries.Count() )
	int GetGridEntry( const IntVector3D &vIntegerPosition ) const;

	struct LuxelHashEntry_t
	{
		Vector m_vPosition;
		Vector m_vNormal;
		int m_nDataStart;
		int m_nDataLength;
		int m_nNextEntryIndex;
	};

	CUtlVector< ColorRGBExp32 > m_LuxelData;		// Contiguous luxel data
	CUtlVector< LuxelHashEntry_t > m_HashEntries;	// Pool of hash entry linked list nodes, one per luxel
	CUtlVector< dworldlight_t > m_WorldLightsLDR;
	CUtlVector< dworldlight_t > m_WorldLightsHDR;

	CUtlVector< int > m_UniformGrid;				// 3D grid of indices into a linked list of LuxelHashEntry_t (-1 means no entry)
};

#endif // LIGHTMAPTRANSFER_H