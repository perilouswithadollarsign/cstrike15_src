//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================


#include "mdllib_utils.h"


//////////////////////////////////////////////////////////////////////////
//
// CInsertionTracker implementation
//
//////////////////////////////////////////////////////////////////////////


void CInsertionTracker::InsertBytes( void *pos, int length )
{
	if ( length <= 0 )
		return;

	Assert( m_map.InvalidIndex() == m_map.Find( ( byte * ) pos ) );
	m_map.InsertOrReplace( ( byte * ) pos, length );
}

int CInsertionTracker::GetNumBytesInserted() const
{
	int iInserted = 0;

	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		int numBytes = m_map.Element( idx );
		iInserted += numBytes;
	}

	return iInserted;
}

void CInsertionTracker::Finalize()
{
	// Iterate the map and find all the adjacent removal data blocks
	// TODO:
}

void CInsertionTracker::MemMove( void *ptrBase, int &length ) const
{
	int numBytesInsertReq = GetNumBytesInserted();
	byte *pbBlockEnd = BYTE_OFF_PTR( ptrBase, length );
	length += numBytesInsertReq;

	for ( Map::IndexType_t idx = m_map.LastInorder();
		idx != m_map.InvalidIndex(); idx = m_map.PrevInorder( idx ) )
	{
		byte *ptr = m_map.Key( idx );
		int numBytes = m_map.Element( idx );

		// Move [ptr, pbBlockEnd) ->> + numBytesInsertReq
		memmove( BYTE_OFF_PTR( ptr, numBytesInsertReq ), ptr, BYTE_DIFF_PTR( ptr, pbBlockEnd ) );

		// Inserted data
		memset( BYTE_OFF_PTR( ptr, numBytesInsertReq - numBytes ), 0, numBytes );

		numBytesInsertReq -= numBytes;
		pbBlockEnd = ptr;
	}
}

int CInsertionTracker::ComputeOffset( void *ptrBase, int off ) const
{
	void *ptrNewBase = ComputePointer( ptrBase );
	void *ptrNewData = ComputePointer( BYTE_OFF_PTR( ptrBase, off ) );
	return BYTE_DIFF_PTR( ptrNewBase, ptrNewData );
}

void * CInsertionTracker::ComputePointer( void *ptrNothingInserted ) const
{
	int iInserted = 0;

	// Iterate the map and find all the data that would be inserted before the given pointer
	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		if ( m_map.Key( idx ) < ptrNothingInserted )
			iInserted += m_map.Element( idx );
		else
			break;
	}

	return BYTE_OFF_PTR( ptrNothingInserted, iInserted );
}



//////////////////////////////////////////////////////////////////////////
//
// CMemoryMovingTracker implementation
//
//////////////////////////////////////////////////////////////////////////


void CMemoryMovingTracker::RegisterBytes( void *pos, int length )
{
	if ( length <= 0 && m_ePolicy != MEMORY_MODIFY )
		return;

	// -- hint
	if ( m_map.Count() && m_ePolicy == MEMORY_REMOVE )
	{
		if ( m_hint.ptr < pos )
		{
			if ( BYTE_OFF_PTR( m_hint.ptr, m_hint.len ) == pos )
			{
				m_hint.len += length;
				m_map.Element( m_hint.idx ) = m_hint.len;
				return;
			}
		}
		else if ( m_hint.ptr > pos )
		{
			if ( BYTE_OFF_PTR( pos, length ) == m_hint.ptr )
			{
				m_hint.len += length;
				m_hint.ptr = BYTE_OFF_PTR( m_hint.ptr, - length );
				m_map.Key( m_hint.idx ) = m_hint.ptr;
				m_map.Element( m_hint.idx ) = m_hint.len;
				return;
			}
		}
	}
	// -- end hint

	// Insert new
	Assert( m_map.InvalidIndex() == m_map.Find( ( byte * ) pos ) );
	Map::IndexType_t idx = m_map.InsertOrReplace( ( byte * ) pos, length );
	
	// New hint
	m_hint.idx = idx;
	m_hint.ptr = ( byte * ) pos;
	m_hint.len = length;
}

int CMemoryMovingTracker::GetNumBytesRegistered() const
{
	int iRegistered = 0;

	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		int numBytes = m_map.Element( idx );
		if ( m_ePolicy == MEMORY_MODIFY && numBytes <= 0 )
			continue;
		iRegistered += numBytes;
	}

	return iRegistered;
}

void CMemoryMovingTracker::RegisterBaseDelta( void *pOldBase, void *pNewBase )
{
	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		m_map.Key( idx ) = BYTE_OFF_PTR( m_map.Key( idx ), BYTE_DIFF_PTR( pOldBase, pNewBase ) );
	}
	m_hint.ptr = BYTE_OFF_PTR( m_hint.ptr, BYTE_DIFF_PTR( pOldBase, pNewBase ) );
}

void CMemoryMovingTracker::Finalize()
{
	// Iterate the map and find all the adjacent removal data blocks
	// TODO:
}

void CMemoryMovingTracker::MemMove( void *ptrBase, int &length ) const
{
	if ( m_ePolicy == MEMORY_REMOVE )
	{
		int iRemoved = 0;

		for ( Map::IndexType_t idx = m_map.FirstInorder();
			idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
		{
			byte *ptr = m_map.Key( idx );
			int numBytes = m_map.Element( idx );
			byte *ptrDest = BYTE_OFF_PTR( ptr, - iRemoved );
			memmove( ptrDest, BYTE_OFF_PTR( ptrDest, numBytes ), BYTE_DIFF_PTR( BYTE_OFF_PTR( ptr, numBytes ), BYTE_OFF_PTR( ptrBase, length ) ) );
			iRemoved += numBytes;
		}

		length -= iRemoved;
	}

	if ( m_ePolicy == MEMORY_INSERT )
	{
		for ( Map::IndexType_t idx = m_map.LastInorder();
			idx != m_map.InvalidIndex(); idx = m_map.PrevInorder( idx ) )
		{
			byte *ptr = m_map.Key( idx );
			int numBytes = m_map.Element( idx );
			byte *ptrDest = BYTE_OFF_PTR( ptr, numBytes );
			memmove( ptrDest, ptr, BYTE_DIFF_PTR( ptr, BYTE_OFF_PTR( ptrBase, length ) ) );
			length += numBytes;
		}
	}

	if ( m_ePolicy == MEMORY_MODIFY )
	{
		// Perform insertions first:
		for ( Map::IndexType_t idx = m_map.LastInorder();
			idx != m_map.InvalidIndex(); idx = m_map.PrevInorder( idx ) )
		{
			byte *ptr = m_map.Key( idx );
			int numBytes = m_map.Element( idx );
			if ( numBytes <= 0 )
				continue;	// this is removal
			byte *ptrDest = BYTE_OFF_PTR( ptr, numBytes );
			memmove( ptrDest, ptr, BYTE_DIFF_PTR( ptr, BYTE_OFF_PTR( ptrBase, length ) ) );
			length += numBytes;
		}

		// Now perform removals accounting for all insertions
		// that has happened up to the moment
		int numInsertedToPoint = 0;
		int iRemoved = 0;
		for ( Map::IndexType_t idx = m_map.FirstInorder();
			idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
		{
			byte *ptr = m_map.Key( idx );
			int numBytes = m_map.Element( idx );
			if ( numBytes >= 0 )
			{
				numInsertedToPoint += numBytes;
				continue;	// this is insertion that already happened
			}
			numBytes = -numBytes;
			ptr = BYTE_OFF_PTR( ptr, numInsertedToPoint );
			byte *ptrDest = BYTE_OFF_PTR( ptr, - iRemoved );
			memmove( ptrDest, BYTE_OFF_PTR( ptrDest, numBytes ), BYTE_DIFF_PTR( BYTE_OFF_PTR( ptr, numBytes ), BYTE_OFF_PTR( ptrBase, length ) ) );
			iRemoved += numBytes;
		}
		length -= iRemoved;
	}
}

int CMemoryMovingTracker::ComputeOffset( void *ptrBase, int off ) const
{
	void *ptrNewBase = ComputePointer( ptrBase );
	void *ptrNewData = ComputePointer( BYTE_OFF_PTR( ptrBase, off ) );
	return BYTE_DIFF_PTR( ptrNewBase, ptrNewData );
}

void * CMemoryMovingTracker::ComputePointer( void *ptrNothingRemoved ) const
{
	int iAffected = 0;

	// Iterate the map and find all the data that would be removed/inserted before the given pointer
	for ( Map::IndexType_t idx = m_map.FirstInorder();
		idx != m_map.InvalidIndex(); idx = m_map.NextInorder( idx ) )
	{
		if ( m_map.Key( idx ) < ptrNothingRemoved )
			iAffected += m_map.Element( idx );
		else
			break;
	}

	if ( m_ePolicy == MEMORY_REMOVE )
		return BYTE_OFF_PTR( ptrNothingRemoved, - iAffected );

	if ( m_ePolicy == MEMORY_INSERT || m_ePolicy == MEMORY_MODIFY )
		return BYTE_OFF_PTR( ptrNothingRemoved, iAffected );

	return ptrNothingRemoved;
}


