//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef FILEMEMCACHE_H
#define FILEMEMCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier0/basetypes.h"

#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#include <hash_map>

#pragma warning ( disable : 4200 )

class CachedFileData
{
	friend class FileCache;

protected: // Constructed by FileCache
	CachedFileData() {}
	static CachedFileData *Create( char const *szFilename );
	void Free( void );

public:
	static CachedFileData *GetByDataPtr( void const *pvDataPtr );
	
	char const * GetFileName() const;
	void const * GetDataPtr() const;
	int GetDataLen() const;

	int UpdateRefCount( int iDeltaRefCount ) { return m_numRefs += iDeltaRefCount; }

	bool IsValid() const;

protected:
	enum { eHeaderSize = 256 };
	char m_chFilename[256 - 12];
	int m_numRefs;
	int m_numDataBytes;
	int m_signature;
	unsigned char m_data[0]; // file data spans further
};

class FileCache
{
public:
	FileCache();
	~FileCache() { Clear(); }

public:
	CachedFileData *Get( char const *szFilename );
	void Clear( void );

protected:
	struct icmp { bool operator()( char const *x, char const *y ) const { return _stricmp( x, y ) < 0; } };
	typedef stdext::hash_map< char const *, CachedFileData *, stdext::hash_compare< char const *, icmp > > Mapping;
	Mapping m_map;
};

#endif // #ifndef FILEMEMCACHE_H
