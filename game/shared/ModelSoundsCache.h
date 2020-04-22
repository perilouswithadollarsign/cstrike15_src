//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MODELSOUNDSCACHE_H
#define MODELSOUNDSCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlCachedFileData.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

#define MODELSOUNDSCACHE_VERSION		5

class CStudioHdr;


class CModelSoundsCacheListLess
{
public:
	bool Less( const int &lhs, const int &rhs, void *pCtx )
	{
		return lhs < rhs;
	}
};


#pragma pack(1)
class CModelSoundsCache : public IBaseCacheInfo
{
public:
	CUtlSortVector< int, CModelSoundsCacheListLess > sounds;

	CModelSoundsCache();
	CModelSoundsCache( const CModelSoundsCache& src );

	void PrecacheSoundList();

	virtual void Save( CUtlBuffer& buf  );
	virtual void Restore( CUtlBuffer& buf  );
	virtual void Rebuild( char const *filename );

	static void FindOrAddScriptSound( CUtlSortVector< int, CModelSoundsCacheListLess >& sounds, char const *soundname );
	static void BuildAnimationEventSoundList( CStudioHdr *hdr, CUtlSortVector< int, CModelSoundsCacheListLess >& sounds );
private:
	char const *GetSoundName( int index );
};
#pragma pack()

#endif // MODELSOUNDSCACHE_H
