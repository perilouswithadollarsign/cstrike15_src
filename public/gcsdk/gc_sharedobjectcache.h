//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Additional shared object cache functionality for the GC
//
//=============================================================================

#ifndef GC_SHAREDOBJECTCACHE_H
#define GC_SHAREDOBJECTCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "sharedobjectcache.h"

class CMsgSOCacheSubscribed_SubscribedType;

#include "tier0/memdbgon.h"

namespace GCSDK
{

class CCachedSubscriptionMessage;

//----------------------------------------------------------------------------
// Purpose: The part of a shared object cache that handles all objects of a 
//			single type.
//----------------------------------------------------------------------------
class CSharedObjectContext
{
public:
	CSharedObjectContext( const CSteamID & steamIDOwner );

	bool BAddSubscriber( const CSteamID & steamID );
	bool BRemoveSubscriber( const CSteamID & steamID );
	void RemoveAllSubscribers();
	bool BIsSubscribed( const CSteamID & steamID ) { return m_vecSubscribers.Find( steamID ) != m_vecSubscribers.InvalidIndex(); }

	const CUtlVector< CSteamID > & GetSubscribers() const { return m_vecSubscribers; }
	const CSteamID & GetOwner() const { return m_steamIDOwner; }
private:
	CUtlVector<CSteamID>  m_vecSubscribers;
	CUtlVector<int>		  m_vecSubRefCount;
	CSteamID m_steamIDOwner;
};

class CGCSharedObjectTypeCache;

enum ESOTypeFlags
{
	k_SOFlag_SendToNobody		= 0,
	k_ESOFlag_SendToOwningUser		= 1 << 0,  // will go only to the owner of the cache (if that owner is a user)
	k_ESOFlag_SendToOtherUsers		= 1 << 1,  // will go only to users who are not the owner of the cache
	k_SOFlag_SendToGameservers		= 1 << 2,  // will go only to gameservers, regardless of whether they're the owner
	k_SOFlag_LastFlag			= k_SOFlag_SendToGameservers,
};

//----------------------------------------------------------------------------
// Purpose: Filter object used to determine whether a type cache's objects should
//	should be sent to subscribers and whether each object should be sent
//----------------------------------------------------------------------------
class CISubscriberMessageFilter
{
public:
	virtual bool BShouldSendAnyObjectsInCache( CGCSharedObjectTypeCache *pTypeCache, uint32 unFlags ) const = 0;
	virtual bool BShouldSendObject( CSharedObject *pSharedObject, uint32 unFlags ) const = 0;
};


//----------------------------------------------------------------------------
// Purpose: The part of a shared object cache that handles all objects of a 
//			single type.
//----------------------------------------------------------------------------
class CGCSharedObjectTypeCache : public CSharedObjectTypeCache
{
public:

	typedef CSharedObjectTypeCache Base;

	CGCSharedObjectTypeCache( int nTypeID, const CSharedObjectContext & context );
	virtual ~CGCSharedObjectTypeCache();

	virtual bool AddObject( CSharedObject *pObject );
	virtual CSharedObject *RemoveObject( const CSharedObject & soIndex );

	void BuildCacheSubscribedMsg( CMsgSOCacheSubscribed_SubscribedType *pMsgType, uint32 unFlags, const CISubscriberMessageFilter &filter );
	virtual void EnsureCapacity( uint32 nItems );

	static int GetCachedObjectCount( int nTypeID );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif

private:
	static int UpdateCachedObjectCount( int nTypeID, int nDelta );

	const CSharedObjectContext & m_context;

	static CUtlMap<int, int> sm_mapCachedObjectCounts;
};


//----------------------------------------------------------------------------
// Purpose: A cache of a bunch of shared objects of different types. This class
//			is shared between clients, gameservers, and the GC and is 
//			responsible for sending messages from the GC to cause object 
//			creation/destruction/updating on the clients/gameservers.
//----------------------------------------------------------------------------

class CGCSharedObjectCache : public CSharedObjectCache
{
public:
	CGCSharedObjectCache( const CSteamID & steamIDOwner = CSteamID() );
	virtual ~CGCSharedObjectCache();

	const CSteamID & GetOwner() const { return m_context.GetOwner(); }
	const CUtlVector< CSteamID > & GetSubscribers() const { return m_context.GetSubscribers(); }

	CGCSharedObjectTypeCache *GetTypeCache( int nClassID, bool bCreateIfMissing = false ) { return (CGCSharedObjectTypeCache *)GetBaseTypeCache( nClassID, bCreateIfMissing ); }
	CGCSharedObjectTypeCache *GetTypeCache( int nClassID ) const { return (CGCSharedObjectTypeCache *)const_cast<CGCSharedObjectCache*>(this)->GetBaseTypeCache( nClassID, false ); }
	virtual CSharedObjectTypeCache *CreateTypeCache( int nClassID ) const { return new CGCSharedObjectTypeCache( nClassID, m_context ); }

	// returns various singleton objects
	template< typename SOClass_t >
	SOClass_t *GetSingleton() const
	{
		GCSDK::CGCSharedObjectTypeCache *pTypeCache = GetTypeCache( SOClass_t::k_nTypeID );
		if ( pTypeCache && pTypeCache->GetCount() == 1 )
		{
			return (SOClass_t *)pTypeCache->GetObject( 0 );
		}
		return NULL;
	}

	virtual uint32 CalcSendFlags( const CSteamID &steamID ) const;
	virtual const CISubscriberMessageFilter &GetSubscriberMessageFilter();
	virtual void AddObject( CSharedObject *pSharedObject );
	bool BDestroyObject( const CSharedObject & soIndex, bool bRemoveFromDatabase );
	virtual CSharedObject *RemoveObject( const CSharedObject & soIndex );

	template< typename SOClass_t >
	bool BYieldingLoadSchObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & schDefaults );

	template< typename SOClass_t >
	bool BYieldingLoadSchSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & schDefaults );

	template< typename SOClass_t, typename SchClass_t >
	bool BYieldingLoadProtoBufObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead );

	template< typename SOClass_t, typename SchClass_t >
	bool BYieldingLoadProtoBufSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SchClass_t & schDefaults );

	// @todo temporary for trading and item subscriptions (to be removed once we get cross-game trading)
	virtual void SetTradingPartner( const CSteamID &steamID );
	const CSteamID &GetTradingPartner() const { return m_steamIDTradingPartner; }

	void AddSubscriber( const CSteamID & steamID );
	void RemoveSubscriber( const CSteamID & steamID );
	void RemoveAllSubscribers();
	void SendSubscriberMessage( const CSteamID & steamID );
	bool BIsSubscribed( const CSteamID & steamID ) { return m_context.BIsSubscribed( steamID ); }
	void ClearCachedSubscriptionMessage();

	bool BIsDatabaseDirty() const { return m_databaseDirtyList.NumDirtyObjects() > 0; }

	// This will mark the field as dirty for both network and database
	void DirtyObjectField( CSharedObject *pObj, int nFieldIndex );

	// Marks only dirty for network
	void DirtyNetworkObjectField( CSharedObject *pObj, int nFieldIndex );

	// Mark dirty for database
	void DirtyDatabaseObjectField( CSharedObject *pObj, int nFieldIndex );

	void SendNetworkUpdates( CSharedObject *pObj );
	bool BYieldingAddWriteToTransaction( CSharedObject *pObj, CSQLAccess & sqlAccess );
	void SendAllNetworkUpdates();
	void YieldingWriteToDatabase( CSharedObject *pObj );
	uint32 AddAllWritesToTransaction( CSQLAccess & sqlAccess );
	void CleanAllWrites( );

	void SetInWriteback( bool bInWriteback );
	bool GetInWriteback() const { return m_bInWriteback; }
	RTime32 GetWritebackTime() const { return m_unWritebackTime; }

	void SetLRUHandle( uint32 unLRUHandle ) { m_unLRUHandle = unLRUHandle; }
	uint32 GetLRUHandle() const { return m_unLRUHandle; }

	void Dump() const;
	void DumpDirtyObjects() const;
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif

#ifdef DEBUG
	bool IsObjectCached( CSharedObject *pObj, int nTypeID );
	bool IsObjectDirty( CSharedObject *pObj );
#endif

protected:
	virtual void MarkDirty();
	virtual bool BShouldSendToAnyClients( uint32 unFlags ) const;
	CCachedSubscriptionMessage *BuildSubscriberMessage( uint32 unFlags );

	CSteamID m_steamIDTradingPartner;

protected:
	void SendNetworkUpdateInternal( CSharedObject * pObj, const CUtlVector< int > &dirtyFields );
	void SendUnsubscribeMessage( const CSteamID & steamID );

	CSharedObjectContext m_context;
	CSharedObjectDirtyList m_networkDirtyList;
	CSharedObjectDirtyList m_databaseDirtyList;
	bool m_bInWriteback;
	RTime32 m_unWritebackTime;
	uint32 m_unLRUHandle;
	const IProtoBufMsg *m_pCacheToUsersSubscriptionMsg;
	uint32 m_unCachedSubscriptionMsgFlags;
	CCachedSubscriptionMessage *m_pCachedSubscriptionMsg;
};




//----------------------------------------------------------------------------
// Purpose: Loads a list of CSchemaSharedObjects from a result list from a
//			query.
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t >
bool CGCSharedObjectCache::BYieldingLoadSchObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & objDefaults )
{
	if ( NULL == pResultSet )
		return false;

	CGCSharedObjectTypeCache *pTypeCache = GetTypeCache( SOClass_t::k_nTypeID, true );
	pTypeCache->EnsureCapacity( pResultSet->GetRowCount() );
	for( CSQLRecord record( 0, pResultSet ); record.IsValid(); record.NextRow() )
	{
		SOClass_t *pObj = new SOClass_t();
		pObj->Obj() = objDefaults.Obj();
		record.BWriteToRecord( &pObj->Obj(), csRead );
		pTypeCache->AddObject( pObj );
	}

	return true;
}

//----------------------------------------------------------------------------
// Purpose: Loads a single object of a type. If the object is not available,
//	a new object will be created at default values
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t >
bool CGCSharedObjectCache::BYieldingLoadSchSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SOClass_t & objDefaults )
{
	if ( NULL == pResultSet )
		return false;

	if ( pResultSet->GetRowCount() > 1 )
	{
		EmitError( SPEW_SHAREDOBJ, "Multiple rows passed to BYieldingLoadSchSingleton() on type %d\n", objDefaults.GetTypeID() );
		return false;
	}
	else if ( pResultSet->GetRowCount() == 1 )
	{
		return BYieldingLoadSchObjects<SOClass_t>( pResultSet, csRead, objDefaults );
	}
	else
	{
		// Create it if there wasn't one
		SOClass_t *pSchObj = new SOClass_t();
		pSchObj->Obj() = objDefaults.Obj();
		if( !pSchObj->BYieldingAddToDatabase() )
		{
			EmitError( SPEW_SHAREDOBJ, "Unable to add singleton type %d for %s\n", pSchObj->GetTypeID(), GetOwner().Render() );
			return false;
		}
		AddObject( pSchObj );
		return true;
	}
}


//----------------------------------------------------------------------------
// Purpose: Loads a list of CProtoBufSharedObjects from a result list from a
//			query.
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t, typename SchClass_t >
bool CGCSharedObjectCache::BYieldingLoadProtoBufObjects( IGCSQLResultSet *pResultSet, const CColumnSet & csRead )
{
	if ( NULL == pResultSet )
		return false;

	CGCSharedObjectTypeCache *pTypeCache = GetTypeCache( SOClass_t::k_nTypeID, true );
	pTypeCache->EnsureCapacity( pResultSet->GetRowCount() );
	for( CSQLRecord record( 0, pResultSet ); record.IsValid(); record.NextRow() )
	{
		SchClass_t schRecord;
		record.BWriteToRecord( &schRecord, csRead );

		SOClass_t *pObj = new SOClass_t();
		pObj->ReadFromRecord( schRecord );
		pTypeCache->AddObject( pObj );
	}

	return true;
}


//----------------------------------------------------------------------------
// Purpose: Loads a single object of a type. If the object is not available,
//	a new object will be created at default values
// Inputs:	pResultSet - The result set from the SQL query
//			schDefaults - A schema object that defines the values to set in
//				the new objects for fields that were not read in the query.
//				Typically this will be whatever fields were in the WHERE
//				clause of the query.
//			csRead - A columnSet defining the fields that were read in the query.
//----------------------------------------------------------------------------
template< typename SOClass_t, typename SchClass_t >
bool CGCSharedObjectCache::BYieldingLoadProtoBufSingleton( IGCSQLResultSet *pResultSet, const CColumnSet & csRead, const SchClass_t & schDefaults )
{
	if ( NULL == pResultSet )
		return false;

	if ( pResultSet->GetRowCount() > 1 )
	{
		EmitError( SPEW_SHAREDOBJ, "Multiple rows passed to BYieldingLoadProtoBufSingleton() on type %d\n", SOClass_t::k_nTypeID );
		return false;
	}

	// load the duel summary
	SchClass_t schRead;
	CSQLRecord record( 0, pResultSet );
	if( record.IsValid() )
	{
		record.BWriteToRecord( &schRead, csRead );
	}
	else
	{
		CSQLAccess sqlAccess;
		if( !sqlAccess.BYieldingInsertRecord( const_cast<SchClass_t *>( &schDefaults ) ) )
			return false;
		schRead = schDefaults;
	}

	SOClass_t *pSharedObject = new SOClass_t();
	pSharedObject->ReadFromRecord( schRead );
	AddObject( pSharedObject );

	return true;
}


} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif //GC_SHAREDOBJECTCACHE_H
