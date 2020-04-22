//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "datalinker.h"
#include "dbg.h"
#include "memalloc.h"

namespace DataLinker
{
const char g_cFillChar = '\xDE';

Stream::Stream()
{
	m_data.reserve( 16 );
	m_lastTarget = 1; // target 0 and 1 are special targets
	m_alignBits = 3; // alignment 4 is default
	m_size = 0;
	m_stackBlocks.reserve( 4 ); // we'll probably never nest deeper than this
}

Chunk::Chunk( uint reserve, uint offset )
{
	m_offset = offset;
	m_capacity = reserve;
	m_size = 0;
	m_recentSize = 0;
	m_data = ( byte* )MemAlloc_AllocAligned( reserve, 16 );
	memset( m_data, g_cFillChar, reserve );
}

void Chunk::Free()
{
	MemAlloc_FreeAligned( m_data );
}

Stream::~Stream()
{
	for ( std::vector<Chunk>::iterator it = m_data.begin(); it != m_data.end(); ++it )
		it->Free();
}

void* Stream::WriteBytes( uint numBytes )
{
	EnsureAvailable( numBytes );
	Chunk &chunk = m_data.back();
	byte *p = chunk.m_data + chunk.m_size;
	memset( p, 0, numBytes );
	chunk.m_recentSize = m_size;
	chunk.m_size += numBytes;
	m_size += numBytes;
	return p;
}


void Stream::EnsureAvailable( uint addCapacity )
{
	Assert( addCapacity < 0x10000000 ); // we don't support >1Gb of data yet
	if ( m_data.empty() )
	{
		// first allocation
		m_data.push_back( Chunk( MAX( addCapacity*4u, 4096u ), 0 ) );
	}
	else if ( m_data.back().GetAvailable() < addCapacity )
	{
		m_data.push_back( Chunk( MAX( addCapacity*4u, m_data.back().m_capacity*2u ), m_size ) );
	}
}

int Stream::GetOffsetTo( const void *ptr )const
{
	for ( std::vector<Chunk>::const_reverse_iterator it = m_data.rbegin(); it != m_data.rend(); ++it )
	{
		uint offsetOfPtr = ( uint )( ( ( const byte* )ptr ) - it->m_data );
		if ( offsetOfPtr <= it->m_size )
			return offsetOfPtr + it->m_offset;
	}
	return -1; // invalid offset
}


bool Stream::Compile( void *pBuffer )
{
	{
		byte *p = ( byte* )pBuffer;
		uint offset = 0;
		for ( std::vector<Chunk>::const_iterator it = m_data.begin(); it != m_data.end(); ++it )
		{
			Assert( it->m_offset == offset );
			memcpy( p + offset, it->m_data, it->m_size );
			offset += it->m_size;
		}
		Assert( m_size == offset );
	}

	byte *data = ( byte* )pBuffer;

	// for all link sources, try to resolve them
	uint numUnresolved = 0;
	for ( LinkSourceMap::const_iterator it = m_mapSources.begin(); it != m_mapSources.end(); ++it )
	{
		const LinkSourceDesc_t &source = it->second;
		int offsetOfSource = it->first;
		Assert( uint( offsetOfSource ) < m_size ); // check alignment?

		int* pOffset = ( int* )( data + offsetOfSource );
		if ( source.m_targetId == kSpecialTargetNull )
		{
			*pOffset = 0; // special case of target fixed to NULL
		}
		else if ( source.m_targetId < 0 || source.m_targetId > m_lastTarget )
		{
			Msg( "Unresolved link source @0x%X '%s'\n", offsetOfSource, source.m_pDescription );
			numUnresolved ++;
		}
		else
		{
			// try to find the target
			// if special target (id == 0): target is not in the target map; all the information is resolved directly through the LinkSourceDesc_t
			const LinkTargetDesc_t &target = source.m_targetId == kSpecialTargetDefault ? source.m_defaultTarget : m_mapTargets[source.m_targetId];
			switch ( target.m_type )
			{
			case kTargetNull:
				*pOffset = 0; // special case of target set to NULL later
				break;

			case kTargetResolved:

				switch ( source.m_type )
				{
				case kOtRelative32bit:
					*pOffset = target.m_resolvedOffset - offsetOfSource;
					break;
				case kOtRelative16bit:
				{
					int offset = target.m_resolvedOffset - offsetOfSource;
					if ( offset > 0x7FFF || offset < -0x8000 )
					{
						Msg( "Link source @%d '%s' is out of range (%d doesn't fit to 16 bits)\n", offsetOfSource, source.m_pDescription, offset );
						numUnresolved++;
					}
					else
					{
						*( short* )pOffset = ( short )offset;
					}
				}
				break;
				case kOtAbsolute32bit:
					*pOffset = target.m_resolvedOffset;
					break;
				}
				break;

			default:
				Msg( "Unresolved link source @%d '%s'\n", offsetOfSource, source.m_pDescription );
				numUnresolved++;
				break;
			}
		}
	}

	if ( numUnresolved )
	{
		Error( "%u unresolved links found\n", numUnresolved );
		return false;
	}
	else
		return true;
}





void Stream::Align( uint nAlignment, int nOffset )
{
	if ( nAlignment & ( nAlignment - 1 ) )
	{
		Error( "Wrong alignment %d\n", nAlignment );
	}
	//WriteBytes(((m_size+nAlignment-1) & ~(nAlignment-1)) - m_size);
	WriteBytes( ( nOffset - m_size ) & ( nAlignment - 1 ) );
}


void Stream::Link( LinkSource_t linkSource, LinkTarget_t linkTarget, const char *szDescription )
{
	LinkSourceMap::iterator itFind = m_mapSources.find( linkSource.m_offset );
	if ( itFind == m_mapSources.end() )
	{
		Error( "cannot find source @%d - %s\n", linkSource.m_offset, szDescription );
	}

	itFind->second.m_targetId = linkTarget.m_id;
	itFind->second.m_defaultTarget.m_type = kTargetUnresolved; // un-set default target for validation; we are not referring to it any more
	Assert( linkTarget.m_id != kSpecialTargetDefault );
}



LinkSource_t Stream::LinkToHere( int *pOffset, const char *szDescription )
{
	return Link( pOffset, NewTargetHere(), szDescription );
}

LinkSource_t Stream::Link( int32 *pOffset, LinkTarget_t linkTarget, const char *szDescription )
{
	int offsetToOffset = GetOffsetTo( pOffset );
	LinkSource_t linkSource = NewLinkSource( offsetToOffset, kOtRelative32bit, linkTarget.m_id, szDescription );
	return linkSource;
}

void Stream::Link( int32 *pOffset, const void *pTarget )
{
	if ( !pTarget )
	{
		*pOffset = 0;
		return;
	}
	int offsetToOffset = GetOffsetTo( pOffset );
	int offsetToTarget = GetOffsetTo( pTarget );
	if ( offsetToOffset < 0 || offsetToTarget < 0 )
	{
		Error( "Link(%p,%p) can't find offsets (%d,%d)\n", pOffset, pTarget, offsetToOffset, offsetToTarget );
	}
	if ( offsetToOffset == offsetToTarget )
	{
		Warning( "Link(%p,%p) is self-referencing (%d==%d), this is prohibited by this kind of offset. It'll be interpreted as Null offset at runtime\n", pOffset, pTarget, offsetToOffset, offsetToTarget );
	}

	*pOffset = offsetToTarget - offsetToOffset;
}

void Stream::Link( int16 *pOffset, const void *pTarget )
{
	if ( !pTarget )
	{
		*pOffset = 0;
		return;
	}
	int offsetToOffset = GetOffsetTo( pOffset );
	int offsetToTarget = GetOffsetTo( pTarget );
	if ( offsetToOffset < 0 || offsetToTarget < 0 )
	{
		Error( "Link(%p,%p) can't find offsets (%d,%d)\n", pOffset, pTarget, offsetToOffset, offsetToTarget );
	}
	int offset = offsetToTarget - offsetToOffset;
	int16 offset16 = ( int16 )offset;
	if ( offset != ( int )offset16 )
	{
		Error( "Link16(%p,%p) offsets are too far (%d,%d)\n", pOffset, pTarget, offsetToOffset, offsetToTarget );
	}

	*pOffset = offset16;
}



LinkSource_t Stream::NewOffset( int *pOffset, const char *szDescription )
{
	return NewLinkSource( GetOffsetTo( pOffset ), kOtRelative32bit, kSpecialTargetUndefined, szDescription );
}

LinkSource_t Stream::NewLinkSource( int offsetToOffset, OffsetTypeEnum type, int linkTargetId, const char *szDescription )
{
	Assert( !( offsetToOffset & 3 ) );
	if ( offsetToOffset < 0 && offsetToOffset + sizeof( int ) > m_size )
	{
		Error( "Wrong offset spec. Most probably you called an extra Write() before calling WriteOffset() - %s\n", szDescription );
	}

	LinkSource_t linkSource;
	linkSource.m_offset = offsetToOffset;
	LinkSourceDesc_t &lsd = m_mapSources[offsetToOffset];
	lsd.m_targetId = linkTargetId;
	lsd.m_type = type;
	lsd.m_pDescription = szDescription;

	return linkSource;
}


LinkSource_t Stream::WriteOffset( const char *szDescription )
{
	return WriteOffset( LinkTarget_t(), szDescription );
}

LinkSource_t Stream::WriteOffset( LinkTarget_t linkTarget, const char *szDescription )
{
	int offsetToOffset = m_size;
	Write<int32>();
	return NewLinkSource( offsetToOffset, kOtRelative32bit, linkTarget.m_id, szDescription );
}




LinkTarget_t Stream::NewTarget()// create new, unresolved target
{
	LinkTarget_t linkTarget;
	linkTarget.m_id = ++m_lastTarget; // shouldn't be 0 or -1
	return linkTarget;
}


LinkTarget_t Stream::NewTarget( void *pWhere )
{
	int offsetToTarget = GetOffsetTo( pWhere );
	if ( offsetToTarget < 0 )
		Error( "Invalid new target @%p\n", pWhere );
	LinkTarget_t linkTarget = NewTarget();
	SetTarget( linkTarget, offsetToTarget );
	return linkTarget;
}


LinkTarget_t Stream::NewTargetHere() // creates a target right here
{
	LinkTarget_t linkTarget = NewTarget();
	SetTargetHere( linkTarget );
	return linkTarget;
}



void Stream::SetTarget( LinkTarget_t linkTarget, int offsetToTarget ) // sets the given target to point to offsetToTarget
{
	LinkTargetDesc_t &target = m_mapTargets[linkTarget.m_id];
	target.m_type = kTargetResolved;
	target.m_resolvedOffset = offsetToTarget;
}

void Stream::SetTargetHere( LinkTarget_t linkTarget ) // sets the given target to point to right here
{
	if ( linkTarget.m_id > 0 && linkTarget.m_id <= m_lastTarget )
	{
		SetTarget( linkTarget, m_size );
	}
	else
	{
		Warning( "Trying to set invalid target %d to here (absolute offset %d)\n", linkTarget.m_id, m_size );
	}
}


void Stream::SetTargetNull( LinkTarget_t linkTarget ) // set this target to point to NULL
{
	if ( linkTarget.m_id > 0 && linkTarget.m_id <= m_lastTarget )
	{
		m_mapTargets[linkTarget.m_id].m_type = kTargetNull;
	}
	else
	{
		Warning( "Trying to set invalid target %d to null\n", linkTarget.m_id );
	}
}



bool Stream::IsDeclared( LinkTarget_t linkTarget )const
{
	return m_mapTargets.find( linkTarget.m_id ) != m_mapTargets.end();
}


bool Stream::IsSet( LinkTarget_t linkTarget )const
{
	LinkTargetMap::const_iterator itFind = m_mapTargets.find( linkTarget.m_id );
	if ( itFind == m_mapTargets.end() )
		return false; // it can't be set when it hasn't been declared
	return itFind->second.m_resolvedOffset >= 0;
}





bool Stream::IsDefined( LinkSource_t linkSource )const
{
	LinkSourceMap::const_iterator itFind = m_mapSources.find( linkSource.m_offset );
	return itFind != m_mapSources.end();
}


bool Stream::IsLinked( LinkSource_t linkSource )const
{
	LinkSourceMap::const_iterator itFind = m_mapSources.find( linkSource.m_offset );
	if ( itFind == m_mapSources.end() )
		return false;
	return itFind->second.m_targetId >= 0;
}

void Stream::Begin( const char *szName, uint flags )
{
	Block_t newBlock;

	if ( !m_stackBlocks.empty() )
		newBlock.m_name = m_stackBlocks.back().m_name + ":" + szName;
	else
		newBlock.m_name = szName;

	newBlock.m_tell = Tell();

	m_stackBlocks.push_back( newBlock );
}



void Stream::End()
{
	if ( !m_stackBlocks.empty() )
	{
		const Block_t &curBlock = m_stackBlocks.back();
		m_mapBlockStats[curBlock.m_name].m_size += Tell() - curBlock.m_tell;
		m_stackBlocks.pop_back();
	}
}



void Stream::PrintStats()
{
	for ( BlockStatsMap::const_iterator it = m_mapBlockStats.begin(); it != m_mapBlockStats.end(); ++it )
		Msg( "%-60s %7.1f KiB\n", it->first.c_str(), it->second.m_size / 1024. );
	Msg( "%-60s -----------\n", "" );
	Msg( "%-60s %7.1f KiB\n", "DataLinker total stream size", m_size / 1024. );
}



void Stream::ClearStats()
{
	m_mapBlockStats.clear();
}



const char* Stream::WriteAndLinkString( Offset_t<char> *pOffset, const char *pString )
{
	if ( pString )
	{
		StringTableElement_t &ste = m_stringTable[pString];
		if ( !ste.m_pString )
		{
			int nLen = Q_strlen( pString );

			ste.m_nOffset = Tell();
			ste.m_pString = ( char* )this->WriteBytes( nLen + 1 );

			Q_memcpy( ste.m_pString, pString, nLen + 1 );
		}
		Assert( !Q_strcmp( ste.m_pString, pString ) );

		int offsetToOffset = GetOffsetTo( pOffset );
		pOffset->offset = ste.m_nOffset - offsetToOffset;

		return ste.m_pString;
	}
	else
	{
		pOffset->offset = 0; // make the offset NULL
		return NULL;
	}
}


}