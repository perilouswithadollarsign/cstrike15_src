//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A Scene Image file aggregates all the compiled binary VCD files into
// a single file.
//
//=====================================================================================//
#ifndef SCENE_IMAGE_FILE_H
#define SCENE_IMAGE_FILE_H
#ifdef _WIN32
#pragma once
#endif

#include "commonmacros.h"
#include "tier1/checksum_crc.h"

#define SCENE_IMAGE_ID			MAKEID( 'V','S','I','F' )
#define SCENE_IMAGE_VERSION		3

// scene summary: cached calcs for commmon startup queries, variable sized
// note: if you want to add a member to this struct, one way to do it and 
// save memory is to make lastspeech_msecs be a uint16 rather than a uint32
// and store it as a fixed point seconds (say, 8.8) rather than straight 
// msecs; that gives you an additional 16 bits to squeeze into this struct
// without actually increasing memory usage. Override the inline float
// GetDurToSpeechEnd() function if you do this; all game code uses that rather
// than reading the lastspeech_msecs member directly.
struct SceneImageSummary_t
{
	unsigned int	msecs;
	unsigned int	lastspeech_msecs; ///< milliseconds from beginning of vcd to end of last speak event. 
	int				numSounds;
	int				soundStrings[1];	// has numSounds

	// return time in seconds from beginning of scene to end of last Speak event
	inline float GetDurToSpeechEnd( void ) const  { return lastspeech_msecs * 0.001f; }
};

// stored sorted by crc filename for binary search
struct SceneImageEntry_t
{
	CRC32_t	crcFilename;			// expected to be normalized as scenes\???.vcd
	int		nDataOffset;			// offset to dword aligned data from start
	int		nDataLength;
	int		nSceneSummaryOffset;	// offset to summary
};

struct SceneImageHeader_t
{
	int nId;
	int	nVersion;
	int nNumScenes;				// number of scene files
	int	nNumStrings;			// number of unique strings in table
	int nSceneEntryOffset;

	inline const char *String( short iString )
	{
		if ( iString < 0 || iString >= nNumStrings )
		{
			Assert( 0 );
			return NULL;
		}

		// access string table (after header) to access pool
		unsigned int *pTable = (unsigned int *)((byte *)this + sizeof( SceneImageHeader_t ));
		return (char *)this + pTable[iString];
	}
};

#endif // SCENE_IMAGE_FILE_H
