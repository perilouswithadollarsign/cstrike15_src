//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Base class for objects that are kept in synch between client and server
//
//=============================================================================

#ifndef SHAREDOBJECTCACHE_H
#define SHAREDOBJECTCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "soid.h"
#include "sharedobject.h"

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: The part of a shared object cache that handles all objects of a 
//			single type.
//----------------------------------------------------------------------------
class CSharedObjectTypeCache
{
public:
	CSharedObjectTypeCache( int nTypeID );
	virtual ~CSharedObjectTypeCache();

	int GetTypeID() const { return m_nTypeID; }
	uint32 GetCount() const { return m_vecObjects.Count(); }
	CSharedObject *GetObject( uint32 nObj ) { return m_vecObjects[nObj]; }
	const CSharedObject *GetObject( uint32 nObj ) const { return m_vecObjects[nObj]; }

	virtual bool AddObject( CSharedObject *pObject );
	virtual bool AddObjectClean( CSharedObject *pObject );
	virtual CSharedObject *RemoveObject( const CSharedObject & soIndex );
	virtual void RemoveAllObjectsWithoutDeleting();

	virtual void EnsureCapacity( uint32 nItems );

	CSharedObject *FindSharedObject( const CSharedObject & soIndex );
	const CSharedObject *FindSharedObject( const CSharedObject & soIndex ) const;

	virtual void Dump() const;

protected:
	CSharedObject *RemoveObjectByIndex( uint32 nObj );
	bool HasElement( CSharedObject *pSO ) const { return m_vecObjects.HasElement( pSO ); }

private:
	int FindSharedObjectIndex( const CSharedObject & soIndex ) const;

	CSharedObjectVec m_vecObjects;
	int m_nTypeID;
};


//----------------------------------------------------------------------------
// Purpose: A cache of a bunch of shared objects of different types. This class
//			is shared between clients, gameservers, and the GC and is 
//			responsible for sending messages from the GC to cause object 
//			creation/destruction/updating on the clients/gameservers.
//----------------------------------------------------------------------------
class CSharedObjectCache
{
public:
	CSharedObjectCache();
	virtual ~CSharedObjectCache();

	virtual SOID_t GetOwner() const = 0;

	virtual bool AddObject( CSharedObject *pSharedObject );
	virtual bool AddObjectClean( CSharedObject *pSharedObject );
	virtual CSharedObject *RemoveObject( const CSharedObject & soIndex );
	virtual bool RemoveAllObjectsWithoutDeleting();

	//called to find the type cache for the specified class ID. This will return NULL if one does not exist
	const CSharedObjectTypeCache *FindBaseTypeCache( int nClassID ) const;
	//called to find the type cache for the specified class ID. This will return NULL if one does not exist
	CSharedObjectTypeCache *FindBaseTypeCache( int nClassID );
	//called to create the specified class ID. If one exists, this is the same as find, otherwise one will be constructed
	CSharedObjectTypeCache *CreateBaseTypeCache( int nClassID );


	CSharedObject *FindSharedObject( const CSharedObject & soIndex );
	const CSharedObject *FindSharedObject( const CSharedObject & soIndex ) const;

	void SetVersion( uint64 ulVersion ) { m_ulVersion = ulVersion; }
	uint64 GetVersion() const { return m_ulVersion; }
	virtual void MarkDirty() {}

	virtual void Dump() const;

protected:
	virtual CSharedObjectTypeCache *AllocateTypeCache( int nClassID ) const = 0;
	CSharedObjectTypeCache *GetTypeCacheByIndex( int nIndex ) { return m_CacheObjects.IsValidIndex( nIndex ) ? m_CacheObjects.Element( nIndex ) : NULL; }
	int GetTypeCacheCount() const { return m_CacheObjects.Count(); }

	int FirstTypeCacheIndex() const					{ return m_CacheObjects.Count() > 0 ? 0 : m_CacheObjects.InvalidIndex(); }
	int NextTypeCacheIndex( int iCurrent ) const	{ return ( iCurrent + 1 < m_CacheObjects.Count() ) ? iCurrent + 1 : m_CacheObjects.InvalidIndex(); }
	int InvalidTypeCacheIndex() const				{ return m_CacheObjects.InvalidIndex(); }

	uint64 m_ulVersion;
private:
	CUtlVector< CSharedObjectTypeCache * > m_CacheObjects;
};



} // namespace GCSDK


#endif //SHAREDOBJECTCACHE_H

