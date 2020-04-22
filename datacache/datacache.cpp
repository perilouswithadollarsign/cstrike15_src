//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "datacache/idatacache.h"

#if defined( POSIX ) && !defined( _PS3 )
#include <malloc.h>
#endif

#include "tier0/vprof.h"
#include "basetypes.h"
#include "convar.h"
#include "interface.h"
#include "datamanager.h"
#include "utlrbtree.h"
#include "utlhash.h"
#include "utlmap.h"
#include "generichash.h"
#include "filesystem.h"
#include "datacache.h"
#include "utlvector.h"
#include "fmtstr.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef _PS3
#include "tls_ps3.h"
extern uint32 gMinAllocSize;
#endif //_PS3

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CDataCache g_DataCache;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDataCache, IDataCache, DATACACHE_INTERFACE_VERSION, g_DataCache );


//-----------------------------------------------------------------------------
//
// Data Cache class implemenations
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
static ConVar mem_force_flush( "mem_force_flush", "0", 0, "Force cache flush of unlocked resources on every alloc" );
static ConVar mem_force_flush_section( "mem_force_flush_section", "", 0, "Cache section to restrict mem_force_flush" );
static int g_iDontForceFlush;

//-----------------------------------------------------------------------------
// DataCacheItem_t
//-----------------------------------------------------------------------------

DEFINE_FIXEDSIZE_ALLOCATOR_MT( DataCacheItem_t, 4096/sizeof(DataCacheItem_t), CUtlMemoryPool::GROW_SLOW );

void DataCacheItem_t::DestroyResource()
{ 
	if ( pSection )
	{
		pSection->DiscardItemData( this, DC_AGE_DISCARD );
	}
	delete this; 
}


//-----------------------------------------------------------------------------
// CDataCacheSection
//-----------------------------------------------------------------------------

CDataCacheSection::CDataCacheSection( CDataCache *pSharedCache, IDataCacheClient *pClient, const char *pszName )
  :	m_pClient( pClient ),
	m_LRU( pSharedCache->m_LRU ),
	m_mutex( pSharedCache->m_mutex ),
	m_pSharedCache( pSharedCache ),
	m_nFrameUnlockCounter( 0 ),
	m_options( 0 )
{
	memset( m_FrameLocks, 0, sizeof( m_FrameLocks ) );
	memset( &m_status, 0, sizeof(m_status) );
	AssertMsg1( strlen(pszName) <= DC_MAX_CLIENT_NAME, "Cache client name too long \"%s\"", pszName );
	Q_strncpy( szName, pszName, sizeof(szName) );

	for ( int i = 0; i < DC_MAX_THREADS_FRAMELOCKED; i++ )
	{
		FrameLock_t *pFrameLock = new FrameLock_t;
		pFrameLock->m_iThread = i;
		m_FreeFrameLocks.Push( pFrameLock );
	}
}

CDataCacheSection::~CDataCacheSection()
{
	FrameLock_t *pFrameLock;
	while ( ( pFrameLock = m_FreeFrameLocks.Pop() ) != NULL )
	{
		delete pFrameLock;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache size.
//-----------------------------------------------------------------------------
void CDataCacheSection::SetLimits( const DataCacheLimits_t &limits )
{
	m_limits = limits;
	AssertMsg( m_limits.nMinBytes == 0 && m_limits.nMinItems == 0, "Cache minimums not yet implemented" );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const DataCacheLimits_t &CDataCacheSection::GetLimits()
{
	return m_limits;
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache options.
//-----------------------------------------------------------------------------
void CDataCacheSection::SetOptions( unsigned options )
{
	m_options = options;
}

unsigned int CDataCacheSection::GetOptions()
{
	return m_options;
}

//-----------------------------------------------------------------------------
// Purpose: Get the current state of the section
//-----------------------------------------------------------------------------
void CDataCacheSection::GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits )
{
	if ( pStatus )
	{
		*pStatus = m_status;
	}

	if ( pLimits )
	{
		*pLimits = m_limits;
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDataCacheSection::EnsureCapacity( unsigned nBytes, unsigned nItems )
{
	VPROF( "CDataCacheSection::EnsureCapacity" );

	if ( m_limits.nMaxItems != (unsigned)-1 || m_limits.nMaxBytes != (unsigned)-1 )
	{
		unsigned nNewSectionBytes = GetNumBytes() + nBytes;

		if ( nNewSectionBytes > m_limits.nMaxBytes )
		{
			Purge( nNewSectionBytes - m_limits.nMaxBytes );
		}

		if ( GetNumItems() >= m_limits.nMaxItems )
		{
			PurgeItems( ( GetNumItems() - m_limits.nMaxItems ) + 1 );
		}
	}

	m_pSharedCache->EnsureCapacity( nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: Add an item to the cache.  Purges old items if over budget, returns false if item was already in cache.
//-----------------------------------------------------------------------------
bool CDataCacheSection::Add( DataCacheClientID_t clientId, const void *pItemData, unsigned size, DataCacheHandle_t *pHandle )
{
	return AddEx( clientId, pItemData, size, DCAF_DEFAULT, pHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Add an item to the cache.  Purges old items if over budget, returns false if item was already in cache.
//-----------------------------------------------------------------------------
bool CDataCacheSection::AddEx( DataCacheClientID_t clientId, const void *pItemData, unsigned size, unsigned flags, DataCacheHandle_t *pHandle )
{
	VPROF( "CDataCacheSection::Add" );

#if !defined( _CERT )
	ForceFlushDebug( true );
#endif

	if ( ( m_options & DC_VALIDATE ) && Find( clientId ) )
	{
		Error( "Duplicate add to data cache\n" );
		return false;
	}

	EnsureCapacity( size );

	DataCacheItemData_t itemData = 
	{
		pItemData,
		size,
		clientId,
		this
	};

	memhandle_t hMem = m_LRU.CreateResource( itemData, true );
#ifdef _PS3
	if ( m_LRU.LockCount( hMem ) == 1 )
	{
		NoteLock( size );
	}
#endif

	Assert( hMem != (memhandle_t)0 && hMem != (memhandle_t)DC_INVALID_HANDLE );

	AccessItem( hMem )->hLRU = hMem;

	if ( pHandle )
	{
		*pHandle = (DataCacheHandle_t)hMem;
	}

	NoteAdd( size );

	OnAdd( clientId, (DataCacheHandle_t)hMem );

	g_iDontForceFlush++;

	if ( flags & DCAF_LOCK )
	{
		Lock( (DataCacheHandle_t)hMem );
	}
	// Add implies a frame lock. A no-op if not in frame lock
	FrameLock( (DataCacheHandle_t)hMem );

	g_iDontForceFlush--;

	m_LRU.UnlockResource( hMem );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Finds an item in the cache, returns NULL if item is not in cache.
//-----------------------------------------------------------------------------
DataCacheHandle_t CDataCacheSection::Find( DataCacheClientID_t clientId )
{
	VPROF( "CDataCacheSection::Find" );

	m_status.nFindRequests++;

	DataCacheHandle_t hResult = DoFind( clientId );

	if ( hResult != DC_INVALID_HANDLE )
	{
		m_status.nFindHits++;
	}

	return hResult;
}

//---------------------------------------------------------
DataCacheHandle_t CDataCacheSection::DoFind( DataCacheClientID_t clientId )
{
	AUTO_LOCK( m_mutex );
	memhandle_t hCurrent;

	hCurrent = GetFirstUnlockedItem();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->clientId == clientId )
		{
			m_status.nFindHits++;
			return (DataCacheHandle_t)hCurrent;
		}
		hCurrent = GetNextItem( hCurrent );
	}

	hCurrent = GetFirstLockedItem();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->clientId == clientId )
		{
			m_status.nFindHits++;
			return (DataCacheHandle_t)hCurrent;
		}
		hCurrent = GetNextItem( hCurrent );
	}

	return DC_INVALID_HANDLE;
}


//-----------------------------------------------------------------------------
// Purpose: Get an item out of the cache and remove it. No callbacks are executed.
//-----------------------------------------------------------------------------
DataCacheRemoveResult_t CDataCacheSection::Remove( DataCacheHandle_t handle, const void **ppItemData, unsigned *pItemSize, bool bNotify )
{
	VPROF( "CDataCacheSection::Remove" );

	if ( handle != DC_INVALID_HANDLE )
	{
		memhandle_t lruHandle = (memhandle_t)handle;
		if ( m_LRU.LockCount( lruHandle ) > 0 )
		{
			return DC_LOCKED;
		}

		AUTO_LOCK( m_mutex );

		DataCacheItem_t *pItem = AccessItem( lruHandle );
		if ( pItem )
		{
			if ( ppItemData )
			{
				*ppItemData = pItem->pItemData;
			}

			if ( pItemSize )
			{
				*pItemSize = pItem->size;
			}

			DiscardItem( lruHandle, ( bNotify ) ? DC_REMOVED : DC_NONE );

			return DC_OK;
		}
	}

	return DC_NOT_FOUND;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CDataCacheSection::IsPresent( DataCacheHandle_t handle )
{
	return ( m_LRU.GetResource_NoLockNoLRUTouch( (memhandle_t)handle ) != NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Get without locking
//-----------------------------------------------------------------------------
void CDataCacheSection::GetAndLockMultiple( void **ppData, int nCount, DataCacheHandle_t *pHandles )
{
	VPROF( "CDataCacheSection::GetAndLockMultiple" );

#if !defined( _CERT )
	ForceFlushDebug( !g_iDontForceFlush );
#endif

	AUTO_LOCK( m_mutex );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pHandles[i] == DC_INVALID_HANDLE )
		{
			ppData[i] = NULL;
			continue;
		}

		int nLockCount;
		DataCacheItem_t *pItem = m_LRU.LockResourceReturnCount( &nLockCount, (memhandle_t)pHandles[i] );
		if ( !pItem )
		{
			ppData[i] = NULL;
			continue;
		}

		if ( nLockCount == 1 )
		{
			NoteLock( pItem->size );
		}
		ppData[i] = const_cast<void *>( pItem->pItemData );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Lock an item in the cache, returns NULL if item is not in the cache.
//-----------------------------------------------------------------------------
void *CDataCacheSection::Lock( DataCacheHandle_t handle )
{
	VPROF( "CDataCacheSection::Lock" );

#if !defined( _CERT )
	ForceFlushDebug( !g_iDontForceFlush );
#endif

	if ( handle != DC_INVALID_HANDLE )
	{
		int nCount;
		DataCacheItem_t *pItem = m_LRU.LockResourceReturnCount( &nCount, (memhandle_t)handle );
		if ( pItem )
		{
			if ( nCount == 1 )
			{
				NoteLock( pItem->size );
			}
			return const_cast<void *>(pItem->pItemData);
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Unlock a previous lock.
//-----------------------------------------------------------------------------
int CDataCacheSection::Unlock( DataCacheHandle_t handle )
{
	VPROF( "CDataCacheSection::Unlock" );

	int iNewLockCount = 0;
	if ( handle != DC_INVALID_HANDLE )
	{
		AssertMsg( AccessItem( (memhandle_t)handle ) != NULL, "Attempted to unlock nonexistent cache entry" );
		unsigned nBytesUnlocked = 0;
		m_mutex.Lock();
		iNewLockCount = m_LRU.UnlockResource( (memhandle_t)handle );
		if ( iNewLockCount == 0 )
		{
			nBytesUnlocked = AccessItem( (memhandle_t)handle )->size;
		}
		m_mutex.Unlock();
		if ( nBytesUnlocked )
		{
			NoteUnlock( nBytesUnlocked );
			EnsureCapacity( 0 );
		}
	}
	return iNewLockCount;
}


//-----------------------------------------------------------------------------
// Purpose: Lock the mutex
//-----------------------------------------------------------------------------
void CDataCacheSection::LockMutex()
{
	g_iDontForceFlush++;
	m_mutex.Lock();
}


//-----------------------------------------------------------------------------
// Purpose: Unlock the mutex
//-----------------------------------------------------------------------------
void CDataCacheSection::UnlockMutex()
{
	g_iDontForceFlush--;
	m_mutex.Unlock();
}

//-----------------------------------------------------------------------------
// Purpose: Get without locking
//-----------------------------------------------------------------------------
void *CDataCacheSection::Get( DataCacheHandle_t handle, bool bFrameLock )
{
	VPROF( "CDataCacheSection::Get" );

#if !defined( _CERT )
	ForceFlushDebug( !g_iDontForceFlush );
#endif

	if ( handle != DC_INVALID_HANDLE )
	{
		if ( bFrameLock && IsFrameLocking() )
			return FrameLock( handle );

		AUTO_LOCK( m_mutex );
		DataCacheItem_t *pItem = m_LRU.GetResource_NoLock( (memhandle_t)handle );
		if ( pItem )
		{
			return const_cast<void *>( pItem->pItemData );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Get without locking
//-----------------------------------------------------------------------------
void *CDataCacheSection::GetNoTouch( DataCacheHandle_t handle, bool bFrameLock )
{
	VPROF( "CDataCacheSection::GetNoTouch" );

	if ( handle != DC_INVALID_HANDLE )
	{
		if ( bFrameLock && IsFrameLocking() )
			return FrameLock( handle );

		AUTO_LOCK( m_mutex );
		DataCacheItem_t *pItem = m_LRU.GetResource_NoLockNoLRUTouch( (memhandle_t)handle );
		if ( pItem )
		{
			return const_cast<void *>( pItem->pItemData );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: "Frame locking" (not game frame). A crude way to manage locks over relatively 
//			short periods. Does not affect normal locks/unlocks
//-----------------------------------------------------------------------------
int CDataCacheSection::BeginFrameLocking()
{
	int nThreadID = g_nThreadID;
	FrameLock_t *pFrameLock = m_FrameLocks[nThreadID];
	if ( pFrameLock )
	{
		pFrameLock->m_iLock++;
	}
	else
	{
		while ( ( pFrameLock = m_FreeFrameLocks.Pop() ) == NULL )
		{
			ThreadPause();
			ThreadSleep( 1 );
		}
		pFrameLock->m_iLock = 1;
		pFrameLock->m_pFirst = NULL;
		m_FrameLocks[nThreadID] = pFrameLock;

	}
	return pFrameLock->m_iLock;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CDataCacheSection::IsFrameLocking()
{
	FrameLock_t *pFrameLock = m_FrameLocks[g_nThreadID];
	return ( pFrameLock != NULL );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void *CDataCacheSection::FrameLock( DataCacheHandle_t handle )
{
	VPROF( "CDataCacheSection::FrameLock" );

#if !defined( _CERT )
	ForceFlushDebug( !g_iDontForceFlush );
#endif

	void *pResult = NULL;
	FrameLock_t *pFrameLock = m_FrameLocks[g_nThreadID];
	if ( pFrameLock )
	{
		DataCacheItem_t *pItem = m_LRU.LockResource( (memhandle_t)handle );

		if ( pItem )
		{
			int iThread = pFrameLock->m_iThread;
			if ( pItem->pNextFrameLocked[iThread] == DC_NO_NEXT_LOCKED )	
			{
				pItem->pNextFrameLocked[iThread] = pFrameLock->m_pFirst;
				pFrameLock->m_pFirst = pItem;
				Lock( handle );
			}

			pResult = const_cast<void *>(pItem->pItemData);
			m_LRU.UnlockResource( (memhandle_t)handle );
		}
	}

	return pResult;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CDataCacheSection::EndFrameLocking()
{
	int nThread = g_nThreadID;
	FrameLock_t *pFrameLock = m_FrameLocks[nThread];
	Assert( pFrameLock->m_iLock > 0 );

	if ( pFrameLock->m_iLock == 1 )
	{
		VPROF( "CDataCacheSection::EndFrameLocking" );

		if ( pFrameLock->m_pFirst )
		{
			AUTO_LOCK( m_mutex );

			DataCacheItem_t *pItem = pFrameLock->m_pFirst;
			DataCacheItem_t *pNext;
			int iThread = pFrameLock->m_iThread;
			while ( pItem )
			{
				pNext = pItem->pNextFrameLocked[iThread];
				pItem->pNextFrameLocked[iThread] = DC_NO_NEXT_LOCKED;
				Unlock( pItem->hLRU );
				pItem = pNext;
			}
		}

		m_FreeFrameLocks.Push( pFrameLock );
		m_FrameLocks[nThread] = NULL;
		return 0;
	}
	else
	{
		pFrameLock->m_iLock--;
	}
	return pFrameLock->m_iLock;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int *CDataCacheSection::GetFrameUnlockCounterPtr()
{
	return &m_nFrameUnlockCounter;
}


//-----------------------------------------------------------------------------
// Purpose: Lock management, not for the feint of heart
//-----------------------------------------------------------------------------
int CDataCacheSection::GetLockCount( DataCacheHandle_t handle )
{
	return m_LRU.LockCount( (memhandle_t)handle );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CDataCacheSection::BreakLock( DataCacheHandle_t handle )
{
	return m_LRU.BreakLock( (memhandle_t)handle );
}


//-----------------------------------------------------------------------------
// Purpose: Explicitly mark an item as "recently used"
//-----------------------------------------------------------------------------
bool CDataCacheSection::Touch( DataCacheHandle_t handle )
{
	m_LRU.TouchResource( (memhandle_t)handle );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Explicitly mark an item as "least recently used". 
//-----------------------------------------------------------------------------
bool CDataCacheSection::Age( DataCacheHandle_t handle )
{
	m_LRU.MarkAsStale( (memhandle_t)handle );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Empty the cache. Returns bytes released, will remove locked items if force specified
//-----------------------------------------------------------------------------
unsigned CDataCacheSection::Flush( bool bUnlockedOnly, bool bNotify )
{
	VPROF( "CDataCacheSection::Flush" );

	AUTO_LOCK( m_mutex );

	DataCacheNotificationType_t notificationType = ( bNotify )? DC_FLUSH_DISCARD : DC_NONE;

	memhandle_t hCurrent;
	memhandle_t hNext;

	unsigned nBytesFlushed = 0;
	unsigned nBytesCurrent = 0;

	hCurrent = GetFirstUnlockedItem();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		hNext = GetNextItem( hCurrent );
		nBytesCurrent = AccessItem( hCurrent )->size;

		if ( DiscardItem( hCurrent, notificationType ) )
		{
			nBytesFlushed += nBytesCurrent;
		}
		hCurrent = hNext;
	}

	if ( !bUnlockedOnly )
	{
		hCurrent = GetFirstLockedItem();

		while ( hCurrent != INVALID_MEMHANDLE )
		{
			hNext = GetNextItem( hCurrent );
			nBytesCurrent = AccessItem( hCurrent )->size;

			if ( DiscardItem( hCurrent, notificationType ) )
			{
				nBytesFlushed += nBytesCurrent;
			}
			hCurrent = hNext;
		}
	}

	return nBytesFlushed;
}


//-----------------------------------------------------------------------------
// Purpose: Dump the oldest items to free the specified amount of memory. Returns amount actually freed
//-----------------------------------------------------------------------------
unsigned CDataCacheSection::Purge( unsigned nBytes )
{
	VPROF( "CDataCacheSection::Purge" );

	AUTO_LOCK( m_mutex );

	unsigned nBytesPurged = 0;
	unsigned nBytesCurrent = 0;

	memhandle_t hCurrent = GetFirstUnlockedItem();
	memhandle_t hNext;

	while ( hCurrent != INVALID_MEMHANDLE && nBytes > 0 )
	{
        bool bFree = true;
		hNext = GetNextItem( hCurrent );
        if(bFree)
        {
			nBytesCurrent = AccessItem( hCurrent )->size;

			if ( DiscardItem( hCurrent, DC_FLUSH_DISCARD  ) )
			{
				nBytesPurged += nBytesCurrent;
				nBytes -= MIN( nBytesCurrent, nBytes );
            }
		}
		hCurrent = hNext;
	}

	return nBytesPurged;
}

//-----------------------------------------------------------------------------
// Purpose: Dump the oldest items to free the specified number of items. Returns number actually freed
//-----------------------------------------------------------------------------
unsigned CDataCacheSection::PurgeItems( unsigned nItems )
{
	AUTO_LOCK( m_mutex );

	unsigned nPurged = 0;

	memhandle_t hCurrent = GetFirstUnlockedItem();
	memhandle_t hNext;

	while ( hCurrent != INVALID_MEMHANDLE && nItems )
	{
		hNext = GetNextItem( hCurrent );

		if ( DiscardItem( hCurrent, DC_FLUSH_DISCARD ) )
		{
			nItems--;
			nPurged++;
		}
		hCurrent = hNext;
	}

	return nPurged;
}


//-----------------------------------------------------------------------------
// Purpose: Output the state of the section
//-----------------------------------------------------------------------------
void CDataCacheSection::OutputReport( DataCacheReportType_t reportType )
{
	m_pSharedCache->OutputReport( reportType, GetName() );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the size of a specific item
// Input  : handle - 
//			newSize - 
//-----------------------------------------------------------------------------
void CDataCacheSection::UpdateSize( DataCacheHandle_t handle, unsigned int nNewSize )
{
	DataCacheItem_t *pItem = m_LRU.LockResource( (memhandle_t)handle );
	if ( !pItem )
	{
		// If it's gone from memory, size is already irrelevant
		return;
	}

	unsigned oldSize = pItem->size;
	
	if ( oldSize != nNewSize )
	{
		// Update the size
		pItem->size = nNewSize;

		int bytesAdded = nNewSize - oldSize;
		// If change would grow cache size, then purge items until we have room
		if ( bytesAdded > 0 )
		{
			m_pSharedCache->EnsureCapacity( bytesAdded );
		}
		
		m_LRU.NotifySizeChanged( (memhandle_t)handle, oldSize, nNewSize );
		NoteSizeChanged( oldSize, nNewSize );
	}

	m_LRU.UnlockResource( (memhandle_t)handle );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
memhandle_t CDataCacheSection::GetFirstUnlockedItem()
{
	memhandle_t hCurrent;

	hCurrent = m_LRU.GetFirstUnlocked();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->pSection == this )
		{
			return hCurrent;
		}
		hCurrent = m_LRU.GetNext( hCurrent );
	}
	return INVALID_MEMHANDLE;
}


memhandle_t CDataCacheSection::GetFirstLockedItem()
{
	memhandle_t hCurrent;

	hCurrent = m_LRU.GetFirstLocked();

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->pSection == this )
		{
			return hCurrent;
		}
		hCurrent = m_LRU.GetNext( hCurrent );
	}
	return INVALID_MEMHANDLE;
}


memhandle_t CDataCacheSection::GetNextItem( memhandle_t hCurrent )
{
	hCurrent = m_LRU.GetNext( hCurrent );

	while ( hCurrent != INVALID_MEMHANDLE )
	{
		if ( AccessItem( hCurrent )->pSection == this )
		{
			return hCurrent;
		}
		hCurrent = m_LRU.GetNext( hCurrent );
	}
	return INVALID_MEMHANDLE;
}

bool CDataCacheSection::DiscardItem( memhandle_t hItem, DataCacheNotificationType_t type )
{
	DataCacheItem_t *pItem = AccessItem( hItem );
	if ( DiscardItemData( pItem, type ) )
	{
		if ( m_LRU.LockCount( hItem ) )
		{
			m_LRU.BreakLock( hItem );
			NoteUnlock( pItem->size );
		}

		FrameLock_t *pFrameLock = m_FrameLocks[g_nThreadID];
		if ( pFrameLock )
		{
			int iThread = pFrameLock->m_iThread;
			if ( pItem->pNextFrameLocked[iThread] != DC_NO_NEXT_LOCKED )
			{
				if ( pFrameLock->m_pFirst == pItem )
				{
					pFrameLock->m_pFirst = pItem->pNextFrameLocked[iThread];
				}
				else
				{
					DataCacheItem_t *pCurrent = pFrameLock->m_pFirst;
					while ( pCurrent )
					{
						if ( pCurrent->pNextFrameLocked[iThread] == pItem )
						{
							pCurrent->pNextFrameLocked[iThread] = pItem->pNextFrameLocked[iThread];
							break;
						}
						pCurrent = pCurrent->pNextFrameLocked[iThread];
					}
				}
				pItem->pNextFrameLocked[iThread] = DC_NO_NEXT_LOCKED;
			}

		}

#ifdef _DEBUG
		for ( int i = 0; i < DC_MAX_THREADS_FRAMELOCKED; i++ )
		{
			if ( pItem->pNextFrameLocked[i] != DC_NO_NEXT_LOCKED )
			{
				DebuggerBreak(); // higher level code needs to handle better
			}
		}
#endif

		pItem->pSection = NULL; // inhibit callbacks from lower level resource system
		m_LRU.DestroyResource( hItem );
		return true;
	}
	return false;
}

bool CDataCacheSection::DiscardItemData( DataCacheItem_t *pItem, DataCacheNotificationType_t type )
{
	if ( pItem )
	{
		if ( type != DC_NONE )
		{
			Assert( type == DC_AGE_DISCARD || type == DC_FLUSH_DISCARD || DC_REMOVED );

			if ( type == DC_AGE_DISCARD && m_pSharedCache->IsInFlush() )
				type = DC_FLUSH_DISCARD;

			DataCacheNotification_t notification =
			{
				type,
				GetName(),
				pItem->clientId,
				pItem->pItemData,
				pItem->size
			};

			bool bResult = m_pClient->HandleCacheNotification( notification );
			AssertMsg( bResult, "Refusal of cache drop not yet implemented!" );

			if ( bResult )
			{
				NoteRemove( pItem->size );
			}

			return bResult;
		}

		OnRemove( pItem->clientId );

		pItem->pSection = NULL;
		pItem->pItemData = NULL,
		pItem->clientId = 0;

		NoteRemove( pItem->size );

		return true;
	}
	return false;
}

void CDataCacheSection::ForceFlushDebug( bool bFlush )
{
	if ( bFlush && mem_force_flush.GetBool() )
	{
		if ( !*mem_force_flush_section.GetString() )
		{
			if ( IsGameConsole() )
			{
				// The 360 does not use LRU purge behavior on some sections (section limits are -1) and thus cannot handle arbitrary purges.
				// Instead the 360 marks those sections, and then must iterate/skip here
				int count = m_pSharedCache->GetSectionCount();
				for ( int i = 0; i < count; i++ )
				{
					IDataCacheSection *pCacheSection = m_pSharedCache->FindSection( m_pSharedCache->GetSectionName( i ) );
					if ( pCacheSection && !( pCacheSection->GetOptions() & DC_NO_USER_FORCE_FLUSH ) )
					{
						pCacheSection->Flush();
					}
				}
			}
			else
			{
				m_pSharedCache->Flush();
			}
		}
		else if ( stricmp( szName, mem_force_flush_section.GetString() ) == 0 )
		{
			// allowing user to ignore safety barrier and force flush
			Flush();
		}
	}
}

//-----------------------------------------------------------------------------
// CDataCacheSectionFastFind
//-----------------------------------------------------------------------------
DataCacheHandle_t CDataCacheSectionFastFind::DoFind( DataCacheClientID_t clientId ) 
{ 
	AUTO_LOCK( m_mutex );
	UtlHashFastHandle_t hHash = m_Handles.Find( Hash4( &clientId ) );
	if( hHash != m_Handles.InvalidHandle() )
		return m_Handles[hHash];
	return DC_INVALID_HANDLE; 
}


void CDataCacheSectionFastFind::OnAdd( DataCacheClientID_t clientId, DataCacheHandle_t hCacheItem ) 
{
	AUTO_LOCK( m_mutex );
	Assert( m_Handles.Find( Hash4( &clientId ) ) == m_Handles.InvalidHandle());
	m_Handles.FastInsert( Hash4( &clientId ), hCacheItem );
}


void CDataCacheSectionFastFind::OnRemove( DataCacheClientID_t clientId ) 
{
	AUTO_LOCK( m_mutex );
	UtlHashFastHandle_t hHash = m_Handles.Find( Hash4( &clientId ) );
	Assert( hHash != m_Handles.InvalidHandle());
	if( hHash != m_Handles.InvalidHandle() )
		return m_Handles.Remove( hHash );
}


//-----------------------------------------------------------------------------
// CDataCache
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Convar callback to change data cache 
//-----------------------------------------------------------------------------
void DataCacheSize_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	int nOldValue = (int)flOldValue;
	if ( var.GetInt() != nOldValue )
	{
		g_DataCache.SetSize( var.GetInt() * 1024 * 1024 );
	}
}
ConVar datacachesize( "datacachesize", "32", 0, "Size in MB.", true, 0, true, 128, DataCacheSize_f );

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CDataCache::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_DataCache.SetSize( datacachesize.GetInt() * 1024 * 1024 );
	g_pDataCache = this;

	return true;
}

void CDataCache::Disconnect()
{
	g_pDataCache = NULL;
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Init, Shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CDataCache::Init( void )
{
	return BaseClass::Init();
}

void CDataCache::Shutdown( void )
{
	Flush( false, false );
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CDataCache::QueryInterface( const char *pInterfaceName )
{
	// Loading the datacache DLL mounts *all* interfaces
	// This includes the backward-compatible interfaces + IStudioDataCache
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDataCache::CDataCache()
	: m_mutex( m_LRU.AccessMutex() )
{
	memset( &m_status, 0, sizeof(m_status) );
	m_bInFlush = false;
	m_LRU.SetFreeOnDestruct( false ); // Causes problems in error shut down scenarios as CDataCache::Shutdown() isn't called, so the LRU is pointing to things owned by unloaded DLLs
}

//-----------------------------------------------------------------------------
// Purpose: Controls cache size.
//-----------------------------------------------------------------------------
void CDataCache::SetSize( int nMaxBytes )
{
	m_LRU.SetTargetSize( nMaxBytes );
	m_LRU.FlushToTargetSize();
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache options.
//-----------------------------------------------------------------------------
void CDataCache::SetOptions( unsigned options )
{
	for ( int i = 0; m_Sections.Count(); i++ )
	{
		m_Sections[i]->SetOptions( options );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Controls cache section size.
//-----------------------------------------------------------------------------
void CDataCache::SetSectionLimits( const char *pszSectionName, const DataCacheLimits_t &limits )
{
	IDataCacheSection *pSection = FindSection( pszSectionName );

	if ( !pSection )
	{
		DevMsg( "Cannot find requested cache section \"%s\"", pszSectionName );
		return;
	}

	pSection->SetLimits( limits );
}


//-----------------------------------------------------------------------------
// Purpose: Get the current state of the cache
//-----------------------------------------------------------------------------
void CDataCache::GetStatus( DataCacheStatus_t *pStatus, DataCacheLimits_t *pLimits )
{
	if ( pStatus )
	{
		*pStatus = m_status;
	}

	if ( pLimits )
	{
		Construct( pLimits );
		pLimits->nMaxBytes = m_LRU.TargetSize();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add a section to the cache
//-----------------------------------------------------------------------------
IDataCacheSection *CDataCache::AddSection( IDataCacheClient *pClient, const char *pszSectionName, const DataCacheLimits_t &limits, bool bSupportFastFind )
{
	CDataCacheSection *pSection;

	pSection = (CDataCacheSection *)FindSection( pszSectionName );
	if ( pSection )
	{
		AssertMsg1( pSection->GetClient() == pClient, "Duplicate cache section name \"%s\"", pszSectionName );
		return pSection;
	}

	if ( !bSupportFastFind )
		pSection = new CDataCacheSection( this, pClient, pszSectionName );
	else
		pSection = new CDataCacheSectionFastFind( this, pClient, pszSectionName );
		
	pSection->SetLimits( limits );

	m_Sections.AddToTail( pSection );
	return pSection;	
}


//-----------------------------------------------------------------------------
// Purpose: Remove a section from the cache
//-----------------------------------------------------------------------------
void CDataCache::RemoveSection( const char *pszClientName, bool bCallFlush )
{
#ifdef _PS3
    // TODO: if (!g_bDoingExitSequence)
#endif
    {
		int iSection = FindSectionIndex( pszClientName );

		if ( iSection != m_Sections.InvalidIndex() )
		{
			if ( bCallFlush )
			{
				m_Sections[iSection]->Flush( false );
			}
			delete m_Sections[iSection];
			m_Sections.FastRemove( iSection );
			return;
        }
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find a section of the cache
//-----------------------------------------------------------------------------
IDataCacheSection *CDataCache::FindSection( const char *pszClientName )
{
	int iSection = FindSectionIndex( pszClientName );

	if ( iSection != m_Sections.InvalidIndex() )
	{
		return m_Sections[iSection];
	}
	return NULL;	
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDataCache::EnsureCapacity( unsigned nBytes )
{
	VPROF( "CDataCache::EnsureCapacity" );

	m_LRU.EnsureCapacity( nBytes );
}


//-----------------------------------------------------------------------------
// Purpose: Dump the oldest items to free the specified amount of memory. Returns amount actually freed
//-----------------------------------------------------------------------------
unsigned CDataCache::Purge( unsigned nBytes )
{
	VPROF( "CDataCache::Purge" );

	return m_LRU.Purge( nBytes );
}


//-----------------------------------------------------------------------------
// Purpose: Empty the cache. Returns bytes released, will remove locked items if force specified
//-----------------------------------------------------------------------------
unsigned CDataCache::Flush( bool bUnlockedOnly, bool bNotify )
{
	VPROF( "CDataCache::Flush" );

	unsigned result;

	if ( m_bInFlush )
	{
		return 0;
	}

	m_bInFlush = true;

	if ( bUnlockedOnly )
	{
		result =  m_LRU.FlushAllUnlocked();
	}
	else
	{
		result = m_LRU.FlushAll();
	}

	m_bInFlush = false;

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: Output the state of the cache
//-----------------------------------------------------------------------------
void CDataCache::OutputReport( DataCacheReportType_t reportType, const char *pszSection )
{
	float percent;
	int i;

	AUTO_LOCK( m_mutex );
	int bytesUsed = m_LRU.UsedSize();
	int bytesTotal = m_LRU.TargetSize();
	percent = 100.0f * (float)bytesUsed / (float)bytesTotal;

	CUtlVector<memhandle_t> lruList, lockedlist;

	m_LRU.GetLockHandleList( lockedlist );
	m_LRU.GetLRUHandleList( lruList );

	CDataCacheSection *pSection = NULL;
	if ( pszSection )
	{
		pSection = (CDataCacheSection *)FindSection( pszSection );
		if ( !pSection )
		{
			Msg( "Unknown cache section %s\n", pszSection );
			return;
		}
	}

	if ( reportType == DC_DETAIL_REPORT )
	{
		CUtlRBTree< memhandle_t, int >	sortedbysize( 0, 0, SortMemhandlesBySizeLessFunc );
		for ( i = 0; i < lockedlist.Count(); ++i )
		{
			if ( !pSection || AccessItem( lockedlist[ i ] )->pSection == pSection )
				sortedbysize.Insert( lockedlist[ i ] );
		}

		for ( i = 0; i < lruList.Count(); ++i )
		{
			if ( !pSection || AccessItem( lruList[ i ] )->pSection == pSection )
				sortedbysize.Insert( lruList[ i ] );
		}

		for ( i = sortedbysize.FirstInorder(); i != sortedbysize.InvalidIndex(); i = sortedbysize.NextInorder( i ) )
		{
			OutputItemReport( sortedbysize[ i ] );
		}
		OutputReport( DC_SUMMARY_REPORT, pszSection );
	}
	else if ( reportType == DC_DETAIL_REPORT_LRU )
	{
		for ( i = 0; i < lockedlist.Count(); ++i )
		{
			if ( !pSection || AccessItem( lockedlist[ i ] )->pSection == pSection )
				OutputItemReport( lockedlist[ i ] );
		}

		for ( i = 0; i < lruList.Count(); ++i )
		{
			if ( !pSection || AccessItem( lruList[ i ] )->pSection == pSection )
				OutputItemReport( lruList[ i ] );
		}
		OutputReport( DC_SUMMARY_REPORT, pszSection );
	}
#if defined( _X360 )
	else if ( reportType == DC_DETAIL_REPORT_VXCONSOLE )
	{
		int numLockedItems = lockedlist.Count();
		int numLruItems = lruList.Count();
		CUtlVector< xDataCacheItem_t > vxconsoleItems;
		vxconsoleItems.SetCount( numLockedItems + numLruItems );

		for ( i = 0; i < numLockedItems; ++i )
		{
			OutputItemReport( lockedlist[i], &vxconsoleItems[i] );
		}
		for ( i = 0; i < numLruItems; ++i )
		{
			OutputItemReport( lruList[i], &vxconsoleItems[numLockedItems + i] );
		}

		XBX_rDataCacheList( vxconsoleItems.Count(), vxconsoleItems.Base() );
	}
#endif
	else if ( reportType == DC_SUMMARY_REPORT )
	{
		if ( !pszSection )
		{
			// summary for all of the sections
			for ( int i = 0; i < m_Sections.Count(); ++i )
			{
				if ( m_Sections[i]->GetName() )
				{
					OutputReport( DC_SUMMARY_REPORT, m_Sections[i]->GetName() );
				}
			}
			Msg( "Summary: %i resources total %s, %.2f %% of capacity\n", lockedlist.Count() + lruList.Count(), Q_pretifymem( bytesUsed, 2, true ), percent );
		}
		else
		{
			// summary for the specified section
			DataCacheItem_t *pItem;
			int sectionBytes = 0;
			int sectionCount = 0;
			for ( i = 0; i < lockedlist.Count(); ++i )
			{
				if ( AccessItem( lockedlist[ i ] )->pSection == pSection )
				{
					pItem = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( lockedlist[i] );
					sectionBytes += pItem->size;
					sectionCount++;
				}
			}
			for ( i = 0; i < lruList.Count(); ++i )
			{
				if ( AccessItem( lruList[ i ] )->pSection == pSection )
				{
					pItem = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( lruList[i] );
					sectionBytes += pItem->size;
					sectionCount++;
				}
			}
			int sectionSize = 1;
			float sectionPercent;
			if ( pSection->GetLimits().nMaxBytes == (unsigned int)-1 )
			{
				// section unrestricted, base on total size
				sectionSize = bytesTotal;
			}
			else if ( pSection->GetLimits().nMaxBytes )
			{
				sectionSize = pSection->GetLimits().nMaxBytes;
			}
			sectionPercent = 100.0f * (float)sectionBytes/(float)sectionSize;
			Msg( "Section [%s]: %i resources total %s, %.2f %% of limit (%s)\n", pszSection, sectionCount, Q_pretifymem( sectionBytes, 2, true ), sectionPercent, Q_pretifymem( sectionSize, 2, true ) );
		}
	}
}

//-------------------------------------

void CDataCache::OutputItemReport( memhandle_t hItem, void *pXboxData )
{
	AUTO_LOCK( m_mutex );
	DataCacheItem_t *pItem = m_LRU.GetResource_NoLockNoLRUTouch( hItem );
	if ( !pItem )
		return;

	CDataCacheSection *pSection = pItem->pSection;

	char name[DC_MAX_ITEM_NAME+1];

	name[0] = 0;
	pSection->GetClient()->GetItemName( pItem->clientId, pItem->pItemData, name, DC_MAX_ITEM_NAME );

#if defined( _X360 )
	if ( pXboxData )
	{
		// spew into vxconsole friendly structure
		xDataCacheItem_t *pXboxItem = (xDataCacheItem_t *)pXboxData;
		V_strncpy( pXboxItem->name, name, sizeof( pXboxItem->section ) );
		V_strncpy( pXboxItem->section, pItem->pSection->GetName(), sizeof( pXboxItem->section ) );
		pXboxItem->size = pItem->size;
		pXboxItem->lockCount = m_LRU.LockCount( hItem );
		pXboxItem->clientId = pItem->clientId;
		pXboxItem->itemData = (unsigned int)pItem->pItemData;
		pXboxItem->handle = (unsigned int)hItem;
		return;
	}
#endif

	Msg( "\t%16.16s : %12s : 0x%08x, %p, 0x%p : %s : %s\n", 
		Q_pretifymem( pItem->size, 2, true ), 
		pSection->GetName(), 
		pItem->clientId, pItem->pItemData, hItem,
		( name[0] ) ? name : "unknown",
		( m_LRU.LockCount( hItem ) ) ? CFmtStr( "Locked %d", m_LRU.LockCount( hItem ) ).operator const char*() : "" );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CDataCache::FindSectionIndex( const char *pszSection )
{
	for ( int i = 0; i < m_Sections.Count(); i++ )
	{
		if ( stricmp( m_Sections[i]->GetName(), pszSection ) == 0 )
			return i;
	}
	return m_Sections.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Sorting utility used by the data cache report
//-----------------------------------------------------------------------------
bool CDataCache::SortMemhandlesBySizeLessFunc( const memhandle_t& lhs, const memhandle_t& rhs )
{
	DataCacheItem_t *pItem1 = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( lhs );
	DataCacheItem_t *pItem2 = g_DataCache.m_LRU.GetResource_NoLockNoLRUTouch( rhs );

	Assert( pItem1 );
	Assert( pItem2 );

	return pItem1->size < pItem2->size;
}

int CDataCache::GetSectionCount( void )
{
	return m_Sections.Count();
}

const char *CDataCache::GetSectionName( int iIndex )
{
	if ( iIndex >= 0 && iIndex < m_Sections.Count() )
	{
		return m_Sections[iIndex]->GetName();
	}

	return "";
}
