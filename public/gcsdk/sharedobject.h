//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Base class for objects that are kept in synch between client and server
//
//=============================================================================

#ifndef SHAREDOBJECT_H
#define SHAREDOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#include "utlsortvector.h"
#include "tier0/memdbgon.h"

namespace GCSDK
{

class CSQLAccess;
class CSharedObject;
typedef CSharedObject *(*SOCreationFunc_t)( );
class CSharedObjectCache;



//----------------------------------------------------------------------------
// Purpose: Abstract base class for objects that are shared between the GC and
//			a gameserver/client. These can also be stored in the database.
//----------------------------------------------------------------------------
class CSharedObject
{
	friend class CGCSharedObjectCache;
	friend class CSharedObjectCache;
public:
	virtual ~CSharedObject() {}

	virtual int GetTypeID() const = 0;
	virtual bool BParseFromMessage( const CUtlBuffer & buffer ) = 0;
	virtual bool BParseFromMessage( const std::string &buffer ) = 0;
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate ) = 0;
	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const = 0;
	virtual void Copy( const CSharedObject & soRHS ) = 0;
	virtual void Dump() const = 0;

	virtual bool BAddToMessage( std::string *pBuffer ) const = 0;
	virtual bool BAddDestroyToMessage( std::string *pBuffer ) const = 0;

	bool BIsKeyEqual( const CSharedObject & soRHS ) const;

	static void RegisterFactory( int nTypeID, SOCreationFunc_t fnFactory, uint32 unFlags, const char *pchClassName, const char* pszBuildCacheName, const char* pszCreateName, const char* pszUpdateName );
	static CSharedObject *Create( int nTypeID );
	static uint32 GetTypeFlags( int nTypeID );
	static const char *PchClassName( int nTypeID );
	static const char *PchClassBuildCacheNodeName( int nTypeID );
	static const char *PchClassCreateNodeName( int nTypeID );
	static const char *PchClassUpdateNodeName( int nTypeID );

#ifdef GC
	virtual bool BIsNetworked() const { return true; }
	virtual bool BIsDatabaseBacked() const { return true; }
	virtual bool BYieldingAddToDatabase();
	virtual bool BYieldingWriteToDatabase( const CUtlVector< int > &fields );
	virtual bool BYieldingRemoveFromDatabase();

	virtual bool BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess ) { return false; }
	virtual bool BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) { return false; }
	virtual bool BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess ) { return false; }

	virtual bool BParseFromMemcached( CUtlBuffer & buffer ) { return false; }
	virtual bool BAddToMemcached( CUtlBuffer & bufOutput ) const { return false; }

	bool BSendCreateToSteamIDs( const CUtlVector<CSteamID> & vecRecipients, const SOID_t ownerID, uint64 ulVersion ) const;
	bool BSendDestroyToSteamIDs( const CUtlVector<CSteamID> & vecRecipients, const SOID_t ownerID, uint64 ulVersion ) const;

protected:
/*
	// Dirty bit modification. Do not call these directly on SharedObjects. Call them
	// on the cache that owns the object so they can be added/removed from the right lists.
	virtual void DirtyField( int nField ) = 0;
	virtual void MakeDatabaseClean() = 0;
	virtual void MakeNetworkClean() = 0;
*/
#endif // GC

private:
	struct SharedObjectInfo_t
	{
		int					m_nID;
		uint32				m_unFlags;
		SOCreationFunc_t	m_pFactoryFunction;
		const char *m_pchClassName;
		const char *m_pchBuildCacheSubNodeName;
		const char *m_pchUpdateNodeName;
		const char *m_pchCreateNodeName;
	};

	//compare class that supports sorting shared objects themselves, as well as searching based upon an integer key value
	class CCompareSharedObject
	{
	public:
		bool Less( const SharedObjectInfo_t& lhs, const SharedObjectInfo_t& rhs, void* ) const		{ return lhs.m_nID < rhs.m_nID; }
		bool Less( int lhs, const SharedObjectInfo_t& rhs, void* ) const							{ return lhs < rhs.m_nID; }
		bool Less( const SharedObjectInfo_t& lhs, int rhs, void* ) const							{ return lhs.m_nID < rhs; }
	};

	typedef CUtlSortVector< SharedObjectInfo_t, CCompareSharedObject >	TVecFactories;
	static TVecFactories sm_vecFactories;
	static const SharedObjectInfo_t* FindSharedObjectInfo( int nTypeID );

public:
	static TVecFactories & GetFactories() { return sm_vecFactories; }
};

typedef CUtlVectorFixedGrowable<CSharedObject *, 1> CSharedObjectVec;

//----------------------------------------------------------------------------
// Purpose: Templatized function to use as a factory method for 
//			CSharedObject subclasses
//----------------------------------------------------------------------------
template<typename SharedObjectSubclass_t>
CSharedObject *CreateSharedObjectSubclass()
{
	return new SharedObjectSubclass_t();
}

//internal utility to expand the provided shared object class name into all the names needed for the node
#define REG_SHARED_OBJECT_NAMES_INTERNAL( name )		#name, "BuildCacheSubscribed(" #name ")", "Create(" #name ")", "Update(" #name ")" 

#ifdef GC
#define REG_SHARED_OBJECT_SUBCLASS( derivedClass, flags ) GCSDK::CSharedObject::RegisterFactory( derivedClass::k_nTypeID, GCSDK::CreateSharedObjectSubclass<derivedClass>, (flags), REG_SHARED_OBJECT_NAMES_INTERNAL( derivedClass ) )
#else
#define REG_SHARED_OBJECT_SUBCLASS( derivedClass ) GCSDK::CSharedObject::RegisterFactory( derivedClass::k_nTypeID, GCSDK::CreateSharedObjectSubclass<derivedClass>, 0, REG_SHARED_OBJECT_NAMES_INTERNAL( derivedClass ) )
#endif

} // namespace GCSDK


#include "tier0/memdbgoff.h"

#endif //SHAREDOBJECT_H
