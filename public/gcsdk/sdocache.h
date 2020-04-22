//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Purpose: Serialized Digital Object caching and manipulation
//
//=============================================================================

#ifndef SBOCACHE_H
#define SBOCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlhashmaplarge.h"
#include "tier1/utlqueue.h"
#include "tier1/utlvector.h"

namespace GCSDK
{
// Call to register SDOs. All SDO types must be registered before loaded
#define REG_SDO( classname )	GSDOCache().RegisterSDO( classname::k_eType, #classname )


//-----------------------------------------------------------------------------
// Purpose: Keeps a moving average of a data set
//-----------------------------------------------------------------------------
template< int SAMPLES >
class CMovingAverage
{
public:
	CMovingAverage()
	{
		Reset();
	}

	void Reset()
	{
		memset( m_rglSamples, 0, sizeof( m_rglSamples ) );
		m_cSamples = 0;
		m_lTotal = 0;
	}

	void AddSample( int64 lSample )
	{
		int iIndex = m_cSamples % SAMPLES;
		m_lTotal += ( lSample - m_rglSamples[iIndex] );
		m_rglSamples[iIndex] = lSample;
		m_cSamples++;
	}

	uint64 GetAveragedSample() const
	{
		if ( !m_cSamples )
			return 0;

		int64 iMax = (int64)MIN( m_cSamples, SAMPLES );
		return m_lTotal / iMax;
	}

private:
	int64 m_rglSamples[SAMPLES];
	int64 m_lTotal;
	uint64 m_cSamples;
};


//-----------------------------------------------------------------------------
// Purpose: Global accessor to the manager
//-----------------------------------------------------------------------------
class CSDOCache;
CSDOCache &GSDOCache();


//-----------------------------------------------------------------------------
// Purpose: interface to a Database Backed Object
//-----------------------------------------------------------------------------
class ISDO
{
public:
	virtual ~ISDO() {}

	// Identification
	virtual int GetType() const = 0;
	virtual uint32 GetHashCode() const = 0;
	virtual bool IsEqual( const ISDO *pSDO ) const = 0;

	// Ref counting
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	virtual int GetRefCount() = 0;

	// memory usage
	virtual size_t CubBytesUsed() = 0;

	// Serialization tools
	virtual bool BReadFromBuffer( const byte *pubData, int cubData ) = 0;
	virtual void WriteToBuffer( CUtlBuffer &memBuffer ) = 0;

	// memcached batching tools
	virtual void GetMemcachedKeyName( CUtlString &sName ) = 0;

	// SQL loading
	virtual bool BYldLoadFromSQL( CUtlVector<ISDO *> &vecSDOToLoad, CUtlVector<bool> &vecResults ) const = 0;

	// post-load initialization (whether loaded from SQL or memcached)
	virtual void PostLoadInit() = 0;

	// comparison function for validating memcached copies vs SQL copies
	virtual bool IsIdentical( ISDO *pSDO ) = 0;
};

//**tempcomment**typedef ISDO *(*CreateSDOFunc_t)( uint32 nAccountID );


//-----------------------------------------------------------------------------
// Purpose: base class for a Serialized Digital Object
//-----------------------------------------------------------------------------
template<typename KeyType, int eSDOType, class ProtoMsg>
class CBaseSDO : public ISDO
{
public:
	typedef KeyType KeyType_t;
	enum { k_eType = eSDOType };

	CBaseSDO( const KeyType &key ) : m_Key( key ), m_nRefCount( 0 ) {}

	const KeyType &GetKey() const		{ return m_Key; }

	// ISDO implementation
	virtual int AddRef();
	virtual int Release();
	virtual int GetRefCount();
	virtual int GetType() const			{ return eSDOType; }
	virtual uint32 GetHashCode() const;
	virtual bool BReadFromBuffer( const byte *pubData, int cubData );
	virtual void WriteToBuffer( CUtlBuffer &memBuffer );

	// We use protobufs for all serialization
	virtual void SerializeToProtobuf( ProtoMsg &msg ) = 0;
	virtual bool DeserializeFromProtobuf( const ProtoMsg &msg ) = 0;

	// default comparison function - override to do your own compare
	virtual bool IsEqual( const ISDO *pSDO ) const;

	// default load from SQL is no-op as not all types have permanent storage - override to create a
	// batch load
	virtual bool BYldLoadFromSQL( CUtlVector<ISDO *> &vecSDOToLoad, CUtlVector<bool> &vecResults ) const;

	// override to do initialization after load
	virtual void PostLoadInit() {}

	// compares the serialized versions by default. Override to have more specific behavior
	virtual bool IsIdentical( ISDO *pSDO );

	// tools
	bool WriteToMemcached();
	bool DeleteFromMemcached();

private:
	int m_nRefCount;
	KeyType m_Key;
};


//-----------------------------------------------------------------------------
// Purpose: references to a database-backed object
//			maintains refcount of the object
//-----------------------------------------------------------------------------
template<class T>
class CSDORef
{
	T *m_pSDO;
public:
	CSDORef() { m_pSDO = NULL; }
	explicit CSDORef( CSDORef<T> &SDORef ) { m_pSDO = SDORef.Get(); m_pSDO->AddRef(); }
	explicit CSDORef( T *pSDO ) { m_pSDO = pSDO; if ( m_pSDO ) m_pSDO->AddRef(); }
	~CSDORef() { if ( m_pSDO ) m_pSDO->Release(); }

	T *Get() { return m_pSDO; }
	const T *Get() const { return m_pSDO; }

	T *operator->() { return Get(); }
	const T *operator->() const { return Get(); }

	operator const T *() const	{ return m_pSDO; }
	operator const T *()        { return m_pSDO; }
	operator T *()				{ return m_pSDO; }

	CSDORef<T> &operator=( T *pSDO ) { if ( m_pSDO ) m_pSDO->Release();  m_pSDO = pSDO; if ( m_pSDO ) m_pSDO->AddRef(); return *this; }

	bool operator !() const { return Get() == NULL; }
};


//-----------------------------------------------------------------------------
// Purpose: manages a cache of SDO objects
//-----------------------------------------------------------------------------
class CSDOCache
{
public:
	CSDOCache();
	~CSDOCache();

	// Call to register SDOs. All SDO types must be registered before loaded
	void RegisterSDO( int nType, const char *pchName );

	// A struct to hold stats for the system. This is generated code in Steam. It would be great to make
	// it generated code here if we could bring Steam's operational stats system in the GC
	struct StatsSDOCache_t
	{
		uint64 m_cItemsLRUd;
		uint64 m_cBytesLRUd;
		uint64 m_cItemsUnreferenced;
		uint64 m_cBytesUnreferenced;
		uint64 m_cItemsInCache;
		uint64 m_cBytesInCacheEst;
		uint64 m_cItemsQueuedToLoad;
		uint64 m_cItemsLoadedFromMemcached;
		uint64 m_cItemsLoadedFromSQL;
		uint64 m_cItemsFailedLoadFromSQL;
		uint64 m_cQueuedMemcachedRequests;
		uint64 m_cQueuedSQLRequests;
		uint64 m_nSQLBatchSizeAvgx100;
		uint64 m_nMemcachedBatchSizeAvgx100;
		uint64 m_cSQLRequestsRejectedTooBusy;
		uint64 m_cMemcachedRequestsRejectedTooBusy;
	};

	// loads a SDO, and assigns a reference to it
	// returns false if the item couldn't be loaded, or timed out loading
	template<class T>
	bool BYldLoadSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key, bool *pbTimeoutLoading = NULL );

	// gets access to a SDO, but only if it's currently loaded
	template<class T>
	bool BGetLoadedSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key );

	// starts loading a SDO you're going to reference soon with the above BYldLoadSDO()
	// use this to batch up requests, hinting a set then getting reference to a set is significantly faster
	template<class T>
	void HintLoadSDO( const typename T::KeyType_t &key );
	// as above, but starts load a set
	template<class T>
	void HintLoadSDO( const CUtlVector<typename T::KeyType_t> &vecKeys );

	// force a deletes a SDO from the cache - waits until the object is not referenced
	template<class T>
	bool BYldDeleteSDO( const typename T::KeyType_t &key, uint64 unMicrosecondsToWaitForUnreferenced );

	// SDO refcount management
	void OnSDOReferenced( ISDO *pSDO );
	void OnSDOReleased( ISDO *pSDO );

	// writes a SDO to memcached immediately
	bool WriteSDOToMemcached( ISDO *pSDO );
	// delete the SDO record from memcached
	bool DeleteSDOFromMemcached( ISDO *pSDO );

	// job results
	void OnSDOLoadSuccess( int eSDO, int iRequestID );
	void OnMemcachedSDOLoadFailure( int eSDO, int iRequestID );
	void OnSQLSDOLoadFailure( int eSDO, int iRequestID, bool bSQLLayerSucceeded );
	void OnMemcachedLoadJobComplete( JobID_t jobID );
	void OnSQLLoadJobComplete( int eSDO, JobID_t jobID );

	// test access - deletes all unreferenced objects
	void Flush();

	// stats access
	StatsSDOCache_t &GetStats()	{ return m_StatsSDOCache; }
	int CubReferencedEst();	// number of bytes referenced in the cache

	// prints info about the class
	void Dump();

	// memcached verification - returns the number of mismatches
//**tempcomment**		void YldVerifyMemcachedData( CreateSDOFunc_t pCreateSDOFunc, CUtlVector<uint32> &vecIDs, int *pcMatches, int *pcMismatches );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

	// Functions that need to be in the frame loop
	virtual bool BFrameFuncRunJobsUntilCompleted( CLimitTimer &limitTimer );
	virtual bool BFrameFuncRunMemcachedQueriesUntilCompleted( CLimitTimer &limitTimer );
	virtual bool BFrameFuncRunSQLQueriesOnce( CLimitTimer &limitTimer );

private:
	// Custom comparator for our hash map
	class CDefPISDOEquals
	{
	public:
		CDefPISDOEquals() {}
		CDefPISDOEquals( int i ) {}
		inline bool operator()( const ISDO *lhs, const ISDO *rhs ) const { return ( lhs->IsEqual( rhs ) );	}
		inline bool operator!() const { return false; }
	};

	class CPISDOHashFunctor
	{
	public:
		uint32 operator()(const ISDO *pSDO ) const { return pSDO->GetHashCode(); }
	};

	template<class T>
	int FindLoadedSDO( const typename T::KeyType_t &key );

	template<class T>
	int QueueLoad( const typename T::KeyType_t &key );
	int QueueMemcachedLoad( ISDO *pSDO );

	// items already loaded - Maps the SDO to the LRU position
	CUtlHashMapLarge<ISDO *, int, CDefPISDOEquals, CPISDOHashFunctor> m_mapISDOLoaded;

	// items we have queued to load, in the state of either being loaded from memcached or SQL
	// maps SDO to a list of jobs waiting on the load
	CUtlHashMapLarge<ISDO *, CCopyableUtlVector<JobID_t>, CDefPISDOEquals, CPISDOHashFunctor> m_mapQueuedRequests;

	// requests to load from memcached
	CUtlLinkedList<int, int> m_queueMemcachedRequests;

	// Jobs currently processing memcached load requests
	CUtlVector<JobID_t> m_vecMemcachedJobs;

	// Loading from SQL is divided by SDO type
	struct SQLRequestManager_t
	{
		// requests to load from SQL. Maps to an ID in the map of queued requests
		CUtlLinkedList<int, int> m_queueRequestIDsToLoadFromSQL;

		// SQL jobs we have active doing reads for cache items
		CUtlVector<JobID_t> m_vecSQLJobs;
	};

	// a queue of requests to load from SQL for each type
	CUtlHashMapLarge<int, SQLRequestManager_t *> m_mapQueueSQLRequests;

	// jobs to wake up, since we've satisfied their SDO load request
	struct JobToWake_t
	{
		JobID_t m_jobID;
		bool m_bLoadLayerSuccess;
	};
	CUtlLinkedList<JobToWake_t, int> m_queueJobsToContinue;

	struct LRUItem_t
	{
		ISDO *	m_pSDO;
		size_t	m_cub;
	};
	CUtlLinkedList<LRUItem_t, int> m_listLRU;
	uint32 m_cubLRUItems;
	void RemoveSDOFromLRU( int iMapSDOLoaded );

	struct TypeStats_t
	{
		TypeStats_t() 
			: m_nLoaded( 0 )
			, m_nRefed( 0 )
			, m_cubUnrefed( 0 )
		{}

		CUtlString	m_strName;
		int			m_nLoaded;
		int			m_nRefed;
		int			m_cubUnrefed;
	};

	StatsSDOCache_t m_StatsSDOCache;
	CMovingAverage<100> m_StatMemcachedBatchSize, m_StatSQLBatchSize;
	CUtlMap<int, TypeStats_t> m_mapTypeStats;
};


//-----------------------------------------------------------------------------
// Definition of CBaseSDO template functions now that CSDOCache is defined and
// GSDOCache() can safely be used.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: adds a reference
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
int CBaseSDO<KeyType,ESDOType,ProtoMsg>::AddRef()
{
	if ( ++m_nRefCount == 1 )
		GSDOCache().OnSDOReferenced( this );

	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: releases a reference
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
int CBaseSDO<KeyType,ESDOType,ProtoMsg>::Release()
{
	DbgVerify( m_nRefCount > 0 );

	int nRefCount = --m_nRefCount;

	if ( nRefCount == 0 )
		GSDOCache().OnSDOReleased( this );

	return nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: ref count
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
int CBaseSDO<KeyType,ESDOType,ProtoMsg>::GetRefCount()
{
	return m_nRefCount;
}


//-----------------------------------------------------------------------------
// Purpose: Hashes the object for insertion into a hashtable
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
uint32 CBaseSDO<KeyType,ESDOType,ProtoMsg>::GetHashCode() const
{
	struct hashcode_t
	{
		int m_Type;
		KeyType_t m_Key;
	} hashStruct = { ESDOType, m_Key };
	return PearsonsHashFunctor<hashcode_t>()( hashStruct );
}


//-----------------------------------------------------------------------------
// Purpose: Deserializes the object
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::BReadFromBuffer( const byte *pubData, int cubData )
{
	ProtoMsg msg;
	if ( !msg.ParseFromArray( pubData, cubData ) )
		return false;

	if ( !DeserializeFromProtobuf( msg ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Serializes the object
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
void CBaseSDO<KeyType,ESDOType,ProtoMsg>::WriteToBuffer( CUtlBuffer &memBuffer )
{
	ProtoMsg msg;
	SerializeToProtobuf( msg );
	CProtoBufSharedObjectHelper::AddProtoBufMessageToBuffer( memBuffer, msg, true );
}


//-----------------------------------------------------------------------------
// Purpose: does an immediate write of the object to memcached
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::WriteToMemcached()
{
	return GSDOCache().WriteSDOToMemcached( this );
}


//-----------------------------------------------------------------------------
// Purpose: does an immediate write of the object to memcached
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::DeleteFromMemcached()
{
	return GSDOCache().DeleteSDOFromMemcached( this );
}


//-----------------------------------------------------------------------------
// Purpose: default equality function - compares type and key
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::IsEqual( const ISDO *pSDO ) const
{
	if ( GetType() != pSDO->GetType() )
		return false;

	return ( GetKey() == static_cast<const CBaseSDO<KeyType,ESDOType,ProtoMsg> *>( pSDO )->GetKey() );
}


//-----------------------------------------------------------------------------
// Purpose: Batch load a group of SDO's of the same type from SQL. 
//  Default is no-op as not all types have permanent storage.
//-----------------------------------------------------------------------------
template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::BYldLoadFromSQL( CUtlVector<ISDO *> &vecSDOToLoad, CUtlVector<bool> &vecResults ) const
{
	FOR_EACH_VEC( vecResults, i )
	{
		vecResults[i] = true;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: default validation function - compares serialized versions
//-----------------------------------------------------------------------------
bool CompareSDOObjects( ISDO *pSDO1, ISDO *pSDO2 );

template<typename KeyType, int ESDOType, class ProtoMsg>
bool CBaseSDO<KeyType,ESDOType,ProtoMsg>::IsIdentical( ISDO *pSDO )
{
	return CompareSDOObjects( this, pSDO );
}


//-----------------------------------------------------------------------------
// Purpose: Finds a loaded SDO in memory. Returns the index of the object
//	into the loaded SDOs map
//-----------------------------------------------------------------------------
template<class T>
int CSDOCache::FindLoadedSDO( const typename T::KeyType_t &key )
{
	// see if we have it in cache first
	T probe( key );
	return m_mapISDOLoaded.Find( &probe );
}


//-----------------------------------------------------------------------------
// Purpose: Queues loading an SDO. Returns the index of the entry in the
//	load queue
//-----------------------------------------------------------------------------
template<class T>
int CSDOCache::QueueLoad( const typename T::KeyType_t &key )
{
	T probe( key );
	int iMap = m_mapQueuedRequests.Find( &probe );
	if ( m_mapQueuedRequests.IsValidIndex( iMap ) )
		return iMap;

	return QueueMemcachedLoad( new T( key ) );
}


//-----------------------------------------------------------------------------
// Purpose: Preloads the object into the local cache
//-----------------------------------------------------------------------------
template<class T>
void CSDOCache::HintLoadSDO( const typename T::KeyType_t &key )
{
	// see if we have it in cache first
	if ( !m_mapISDOLoaded.IsValidIndex( FindLoadedSDO<T>( key ) ) )
	{
		QueueLoad<T>( key );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Preloads a set set of objects into the local cache
//-----------------------------------------------------------------------------
template<class T>
void CSDOCache::HintLoadSDO( const CUtlVector<typename T::KeyType_t> &vecKeys )
{
	FOR_EACH_VEC( vecKeys, i )
	{
		HintLoadSDO<T>( vecKeys[i] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns an already-loaded SDO
//-----------------------------------------------------------------------------
template<class T>
bool CSDOCache::BGetLoadedSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key )
{
	int iMap = FindLoadedSDO<T>( key );
	if ( !m_mapISDOLoaded.IsValidIndex( iMap ) )
		return false;

	*pPSDORef = assert_cast<T*>( m_mapISDOLoaded.Key( iMap ) );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Loads the object into memory
//-----------------------------------------------------------------------------
template<class T>
bool CSDOCache::BYldLoadSDO( CSDORef<T> *pPSDORef, const typename T::KeyType_t &key, bool *pbTimeoutLoading /* = NULL */ )
{
	VPROF_BUDGET( "CSDOCache::BYldLoadSDO", VPROF_BUDGETGROUP_STEAM );
	if ( pbTimeoutLoading )
		*pbTimeoutLoading = false;

	// Clear the current object the ref is holding
	*pPSDORef = NULL;

	// see if we have it in cache first
	if ( BGetLoadedSDO( pPSDORef, key ) )
		return true;

	// otherwise batch it for load
	int iMap = QueueLoad<T>( key );

	// make sure we could queue it
	if ( !m_mapQueuedRequests.IsValidIndex( iMap ) )
		return false;

	// add the current job to this list waiting for the object to load 
	m_mapQueuedRequests[iMap].AddToTail( GJobCur().GetJobID() );

	// wait for it to load (loader will signal our job when done)
	if ( !GJobCur().BYieldingWaitForWorkItem() )
	{
		if ( pbTimeoutLoading )
			*pbTimeoutLoading = true;
		return false;
	}

	// should be loaded - look up in the load map and try again
	bool bRet = BGetLoadedSDO( pPSDORef, key );
	Assert( bRet );

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: reloads an existing element from the SQL DB
//-----------------------------------------------------------------------------
template<class T>
bool CSDOCache::BYldDeleteSDO( const typename T::KeyType_t &key, uint64 unMicrosecondsToWaitForUnreferenced )
{
	// see if we have it in cache first
	int iMap = FindLoadedSDO<T>( key );
	if ( !m_mapISDOLoaded.IsValidIndex( iMap ) )
	{
		T temp( key );
		temp.DeleteFromMemcached();
		return true; /* we're good, it's not loaded */
	}

	assert_cast<T *>(m_mapISDOLoaded.Key( iMap ))->DeleteFromMemcached();

	// check the ref count
	int64 cAttempts = MAX( 1LL, (int64)(unMicrosecondsToWaitForUnreferenced / k_cMicroSecPerShellFrame) );
	while ( cAttempts-- > 0 )
	{
		if ( 0 == m_mapISDOLoaded.Key( iMap )->GetRefCount() )
		{
			// delete the object
			Assert( m_listLRU.IsValidIndex( m_mapISDOLoaded[iMap] ) );

			int iMapStats = m_mapTypeStats.Find( m_mapISDOLoaded.Key( iMap )->GetType() );
			if ( m_mapTypeStats.IsValidIndex( iMapStats ) )
			{
				m_mapTypeStats[iMapStats].m_nLoaded--;
			}

			RemoveSDOFromLRU( iMap );
			ISDO *pSDO = m_mapISDOLoaded.Key( iMap );
			m_mapISDOLoaded.RemoveAt( iMap );
			delete pSDO;
			return true;
		}
		else
		{
			GJobCur().BYieldingWaitOneFrame();
		}
	}
	
	// couldn't reload
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: A class to factor out the common code in most SDO SQL loading funcitons
//-----------------------------------------------------------------------------
template<class T>
class CSDOSQLLoadHelper
{
public:
	// Initializes with the vector of objects being loaded
	CSDOSQLLoadHelper( const CUtlVector<ISDO *> *vecSDOToLoad, const char *pchProfileName );

	// Loads all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	bool BYieldingExecuteSingleTable( int nFieldID, CUtlMap<typename T::KeyType_t, CCopyableUtlVector<SCH>, int> *pMapResults );
	// Loads the specified columns for all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	bool BYieldingExecuteSingleTable( int nFieldID, const CColumnSet &csetRead, CUtlMap<typename T::KeyType_t, CCopyableUtlVector<SCH>, int> *pMapResults );

	// Functions to load rows from more than one table at a time

	// Loads all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	void AddTableToQuery( int nFieldID );

	// Loads the specified columns for all rows in the SCH table whose field nFieldID match the key of an SDO being loaded
	template<class SCH>
	void AddTableToQuery( int nFieldID, const CColumnSet &csetRead );

	// Executes the mutli-table query
	bool BYieldingExecute();

	// Gets the results for a table from a multi-table query
	template<class SCH>
	bool BGetResults( int nQuery, CUtlMap<typename T::KeyType_t, CCopyableUtlVector<SCH>, int> *pMapResults );

private:
	CUtlVector<typename T::KeyType_t> m_vecKeys;
	CSQLAccess m_sqlAccess;

	struct Query_t
	{
		Query_t( const CColumnSet &columnSet, int nKeyCol ) : m_ColumnSet( columnSet ), m_nKeyCol( nKeyCol ) {}
		CColumnSet	m_ColumnSet;
		int			m_nKeyCol;
	};

	CUtlVector<Query_t> m_vecQueries;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes with the vector of objects being loaded
//-----------------------------------------------------------------------------
template<class T>
CSDOSQLLoadHelper<T>::CSDOSQLLoadHelper( const CUtlVector<ISDO *> *vecSDOToLoad, const char *pchProfileName )
	: m_vecKeys( 0, vecSDOToLoad->Count() )
{
	FOR_EACH_VEC( *vecSDOToLoad, i )
	{
		m_vecKeys.AddToTail( ( (T*)vecSDOToLoad->Element( i ) )->GetKey() );
	}

	Assert( m_vecKeys.Count() > 0 );
	DbgVerify( m_sqlAccess.BBeginTransaction( pchProfileName ) );
}


//-----------------------------------------------------------------------------
// Purpose: Loads all rows in the SCH table whose field nFieldID match the 
//	key of an SDO being loaded.
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
bool CSDOSQLLoadHelper<T>::BYieldingExecuteSingleTable( int nFieldID, CUtlMap<typename T::KeyType_t, CCopyableUtlVector<SCH>, int> *pMapResults )
{
	static const CColumnSet cSetRead = CColumnSet::Full<SCH>();
	return BYieldingExecuteSingleTable( nFieldID, cSetRead, pMapResults );
}


//-----------------------------------------------------------------------------
// Purpose: Loads the specified columns for all rows in the SCH table whose 
//	field nFieldID match the key of an SDO being loaded
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
bool CSDOSQLLoadHelper<T>::BYieldingExecuteSingleTable( int nFieldID, const CColumnSet &csetRead, CUtlMap<typename T::KeyType_t, CCopyableUtlVector<SCH>, int> *pMapResults )
{
	AddTableToQuery<SCH>( nFieldID, csetRead );
	if ( !BYieldingExecute() )
		return false;

	return BGetResults<SCH>( 0, pMapResults );
}


//-----------------------------------------------------------------------------
// Purpose: Loads all rows in the SCH table whose field nFieldID match the key 
//	of an SDO being loaded
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
void CSDOSQLLoadHelper<T>::AddTableToQuery( int nFieldID )
{
	static const CColumnSet cSetRead = CColumnSet::Full<SCH>();
	AddTableToQuery<SCH>( nFieldID, cSetRead );
}


//-----------------------------------------------------------------------------
// Purpose: Loads the specified columns for all rows in the SCH table whose 
//	field nFieldID match the key of an SDO being loaded
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
void CSDOSQLLoadHelper<T>::AddTableToQuery( int nFieldID, const CColumnSet &csetRead )
{
	Assert( csetRead.GetRecordInfo() == GSchemaFull().GetSchema( SCH::k_iTable ).GetRecordInfo() );

	// Bind the params
	FOR_EACH_VEC( m_vecKeys, i )
	{
		m_sqlAccess.AddBindParam( m_vecKeys[i] );
	}

	// Build the query
	CFmtStr1024 sCommand;
	const char *pchColumnName = GSchemaFull().GetSchema( SCH::k_iTable ).GetRecordInfo()->GetColumnInfo( nFieldID ).GetName();

	BuildSelectStatementText( &sCommand, csetRead );
	sCommand.AppendFormat( " WHERE %s IN (%.*s)", pchColumnName, max( 0, ( m_vecKeys.Count() * 2 ) - 1 ), GetInsertArgString() );

	// Execute. Because we're in a transaction this will delay to the commit
	DbgVerify( m_sqlAccess.BYieldingExecute( NULL, sCommand ) );

	m_vecQueries.AddToTail( Query_t( csetRead, nFieldID ) );
}


//-----------------------------------------------------------------------------
// Purpose: Executes the mutli-table query
//-----------------------------------------------------------------------------
template<class T>
bool CSDOSQLLoadHelper<T>::BYieldingExecute()
{
	if ( 0 == m_vecKeys.Count() )
	{
		m_sqlAccess.RollbackTransaction();
		return false;
	}

	if ( !m_sqlAccess.BCommitTransaction() )
		return false;

	Assert( (uint32)m_vecQueries.Count() == m_sqlAccess.GetResultSetCount() );
	return (uint32)m_vecQueries.Count() == m_sqlAccess.GetResultSetCount();
}


//-----------------------------------------------------------------------------
// Purpose: Gets the results for a table from a multi-table query
//-----------------------------------------------------------------------------
template<class T>
template<class SCH>
bool CSDOSQLLoadHelper<T>::BGetResults( int nQuery, CUtlMap<typename T::KeyType_t, CCopyableUtlVector<SCH>, int> *pMapResults )
{
	pMapResults->RemoveAll();

	IGCSQLResultSetList *pResults = m_sqlAccess.GetResults();
	Assert( pResults && nQuery >= 0 && (uint32)nQuery < pResults->GetResultSetCount() && pResults->GetResultSetCount() == (uint32)m_vecQueries.Count() );
	if ( NULL == pResults || nQuery < 0 || (uint32)nQuery >= pResults->GetResultSetCount() || pResults->GetResultSetCount() != (uint32)m_vecQueries.Count() )
		return false;

	Assert( m_vecQueries[nQuery].m_ColumnSet.GetRecordInfo()->GetTableID() == SCH::k_iTable );
	if ( m_vecQueries[nQuery].m_ColumnSet.GetRecordInfo()->GetTableID() != SCH::k_iTable )
		return false;

	CUtlVector<SCH> vecResults;
	if ( !CopyResultToSchVector( pResults->GetResultSet( nQuery ), m_vecQueries[nQuery].m_ColumnSet, &vecResults ) )
		return false;

	// Make a map that counts how many are in each key so we can intelligently preallocate the result map
	// Copying around vectors of large SCHs could get expensive
	CUtlMap<typename T::KeyType_t, int, int > mapCounts( DefLessFunc( T::KeyType_t ) );
	FOR_EACH_VEC( vecResults, iVec )
	{
		uint8 *pubData;
		uint32 cubData;
		if ( !vecResults[iVec].BGetField( m_vecQueries[nQuery].m_nKeyCol, &pubData, &cubData ) )
			return false;

		Assert( cubData == sizeof( T::KeyType_t ) );
		if ( cubData != sizeof( T::KeyType_t ) )
			return false;

		const T::KeyType_t &key = *((T::KeyType_t *)pubData);
		int iMapCounts = mapCounts.Find( key );
		if ( mapCounts.IsValidIndex( iMapCounts ) )
		{
			mapCounts[iMapCounts]++;
		}
		else
		{
			mapCounts.Insert( key, 1 );
		}
	}

	// Preallocate the results map
	pMapResults->EnsureCapacity( mapCounts.Count() );
	FOR_EACH_MAP_FAST( mapCounts, iMapCount )
	{
		int iMapResult = pMapResults->Insert( mapCounts.Key( iMapCount ) );
		pMapResults->Element( iMapResult ).EnsureCapacity( mapCounts[iMapCount] );
	}

	FOR_EACH_VEC( vecResults, iVec )
	{
		uint8 *pubData;
		uint32 cubData;
		if ( !vecResults[iVec].BGetField( m_vecQueries[nQuery].m_nKeyCol, &pubData, &cubData ) )
			return false;

		Assert( cubData == sizeof( T::KeyType_t ) );
		if ( cubData != sizeof( T::KeyType_t ) )
			return false;

		const T::KeyType_t &key = *((T::KeyType_t *)pubData);
		int iMapResult = pMapResults->Find( key );
		Assert( pMapResults->IsValidIndex( iMapResult ) );
		if ( !pMapResults->IsValidIndex( iMapResult ) )
			continue;

		pMapResults->Element( iMapResult ).AddToTail( vecResults[iVec] );
	}

	return true;
}

} // namespace GCSDK

#endif // SDOCACHE_H
