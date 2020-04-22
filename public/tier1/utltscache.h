//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: thread safe cache template class
//
//===========================================================================//



// a thread-safe cache. Cache queries and updates may be performed on multiple threads at once in a
// lock-free fashion. You must call the "FrameUpdate" function while no one is querying against
// this cache.

#include "tier1/utlintrusivelist.h"

// in order to use this class, KEYTYPE must implement bool IsAcceptable( KEYTYPE const & ), and
// RECORDTYPE must implement CreateCacheData( KEYTYPE const & )
template< class RECORDTYPE, class KEYTYPE, class EXTRACREATEDATA, int nNumCacheLines, int nAssociativity = 4 > class CUtlTSCache
{

public:

	class CCacheEntry : public CAlignedNewDelete<16>
	{
	public:
		CCacheEntry *m_pNext;
		CCacheEntry *m_pNextPending;
		KEYTYPE m_Key;
		RECORDTYPE m_Data;

	};



	~CUtlTSCache( void )
	{
		PerFrameUpdate();									// move pending to free
		for( CCacheEntry *pNode = m_pFreeList; pNode; )
		{
			CCacheEntry *pNext = pNode->m_pNext;
			if ( ! ( ( ( pNode >= m_pInitiallyAllocatedNodes ) && 
					   ( pNode < m_pInitiallyAllocatedNodes + InitialAllocationSize() ) ) ) )
			{
				delete pNode;
			}
			pNode = pNext;
		}
		delete[] m_pInitiallyAllocatedNodes;
	}

	CUtlTSCache( void )
	{
		MEM_ALLOC_CREDIT_CLASS();
		memset( m_pCache, 0, sizeof( m_pCache ) );
		m_pInitiallyAllocatedNodes = new CCacheEntry[ InitialAllocationSize() ];
		m_pFreeList = NULL;
		for( int i = 0; i < InitialAllocationSize(); i++ )
		{
			IntrusiveList::AddToHead( m_pFreeList, m_pInitiallyAllocatedNodes + i );
		}
		m_pPendingFreeList = NULL;
	}

	void PerFrameUpdate( void )
	{
		for( CCacheEntry *pNode = m_pPendingFreeList; pNode; pNode = pNode->m_pNextPending )
		{
			IntrusiveList::AddToHead( m_pFreeList, pNode );
		}
		m_pPendingFreeList = NULL;
	}

	// Invalidate() is NOT thread-safe. If you need a thread-safe invalidate, you're going to need
	// to do something like store a generation count in the key.
	void Invalidate( void )
	{
		for( int i = 0; i < ARRAYSIZE( m_pCache ); i++ )
		{
			CCacheEntry *pNode = m_pCache[i];
			if ( pNode )
			{
				IntrusiveList::AddToHead( m_pFreeList, pNode );
			}
			m_pCache[i] = NULL;
		}
	}

	// lookup a value, maybe add one
	RECORDTYPE *Lookup( KEYTYPE const &key, EXTRACREATEDATA const &extraCreateData )
	{
		// first perform our hash function
		uint nHash = HashItem( key ) % nNumCacheLines;
		
		CCacheEntry **pCacheLine = m_pCache + nHash * nAssociativity;

		// now, see if we have an acceptable node
		CCacheEntry *pOldValue[nAssociativity];
		int nOffset = ( nAssociativity ) ? -1 : 0;
		for( int i = 0; i < nAssociativity; i++ )
		{
			pOldValue[i] = pCacheLine[i];
			if ( pOldValue[i] )
			{
				if ( key.IsAcceptable( pOldValue[i]->m_Key ) )
				{
					return &( pOldValue[i]->m_Data );
				}
			}
			else
			{
				nOffset = i;								// replace empty lines first
			}

		}
		// no acceptable entry. We must generate and replace one. We will use a pseudo-random replacement scheme
		if ( ( nAssociativity > 1 ) && ( nOffset == -1 ) )
		{
			nOffset = ( m_nLineCounter++ ) % nAssociativity;
		}

		// get a node
		CCacheEntry *pNode = GetNode();
		pNode->m_Key = key;
		pNode->m_Data.CreateCacheData( key, extraCreateData );
		
		// we will look for a place to insert this. It is possible that we will find no good place and will just ditch it.
		for( int i = 0; i < nAssociativity; i++ )
		{
			// try to install it.
			if ( ThreadInterlockedAssignPointerIf( ( void * volatile * ) ( pCacheLine + nOffset ), pNode, pOldValue[nOffset] ) )
			{
				// put the old one on the pending free list
				if ( pOldValue[nOffset] )
				{
					IntrusiveList::AddToHeadByFieldTS( m_pPendingFreeList, pOldValue[nOffset], &CCacheEntry::m_pNextPending );
				}
				return &( pNode->m_Data );					// success!
			}
			nOffset++;
			if ( nOffset == nAssociativity )
			{
				nOffset = 0;								// wrap around
			}
		}
		// we failed to install this node into the cache. we'll not bother
		IntrusiveList::AddToHeadByFieldTS( m_pPendingFreeList, pNode, &CCacheEntry::m_pNextPending );
		return &( pNode->m_Data );
	}


	
protected:
	
	int InitialAllocationSize( void )
	{
		return nNumCacheLines * nAssociativity;
	}

	CCacheEntry *GetNode( void )
	{
		CCacheEntry *pNode = IntrusiveList::RemoveHeadTS( m_pFreeList );
		if ( ! pNode )
		{
			pNode = new CCacheEntry;
		}
		return pNode;
	}


	CInterlockedUInt m_nLineCounter;						// for cycling through cache lines

	CCacheEntry *m_pCache[nNumCacheLines * nAssociativity];

	CCacheEntry *m_pFreeList;								// nodes available for the cache
	CCacheEntry *m_pPendingFreeList;							// nodes to be moved to the free list at end of frame

	CCacheEntry *m_pInitiallyAllocatedNodes;

	
};



