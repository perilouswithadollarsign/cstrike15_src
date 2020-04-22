//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATA_LINKER_H
#define DATA_LINKER_H

#include "datalinker_interface.h"
#include <map>
#include <vector>
#include <string>
#include "tier1/UtlStringMap.h"

namespace DataLinker
{


enum LinkTargetEnum
{
	kTargetResolved,
	kTargetUnresolved,
	kTargetNull
};

struct LinkTargetDesc_t
{
	LinkTargetEnum m_type;
	int m_resolvedOffset;
	LinkTargetDesc_t()
	{
		m_resolvedOffset = -1;
		m_type = kTargetUnresolved;
	}
};


struct LinkSourceDesc_t
{
	int m_targetId; // when this is 0, we use the embedded structure
	OffsetTypeEnum m_type;
	const char *m_pDescription;

	LinkTargetDesc_t m_defaultTarget;
};


typedef std::map<int, LinkSourceDesc_t> LinkSourceMap;
typedef std::map<int, LinkTargetDesc_t> LinkTargetMap;

class Chunk
{
public:
	Chunk() {}
	Chunk( uint reserve, uint offset );
	void Free();
	uint GetAvailable()const
	{
		return m_capacity - m_size;
	}

	uint m_offset;
	uint m_size;
	uint m_capacity;
	uint m_recentSize;
	byte *m_data;
};



//////////////////////////////////////////////////////////////////////////
// This writer uses malloc() to allocate memory for the binary data
class Stream: public ILinkStream
{
public:
	Stream();
	~Stream();

	void* WriteBytes( uint numBytes );

	void EnsureAvailable( uint addCapacity );

	void Link( LinkSource_t, LinkTarget_t, const char *szDescription );
	void Align( uint nAlignment, int nOffset = 0 );
	long Tell()
	{
		return m_size;
	}

	LinkSource_t WriteOffset( const char *szDescription );
	LinkSource_t WriteOffset( LinkTarget_t linkTarget, const char *szDescription );
	LinkSource_t WriteNullOffset( const char *szDescription )
	{
		return WriteOffset( LinkTargetNull_t(), szDescription );
	}

	LinkSource_t NewOffset( int *pOffset, const char *szDescription );
	LinkSource_t LinkToHere( int *pOffset, const char *szDescription );
	LinkSource_t Link( int32 *pOffset, LinkTarget_t linkTarget, const char *szDescription );
	void Link( int32 *pOffset, const void *pTarget );
	void Link( int16 *pOffset, const void *pTarget );

	LinkTarget_t NewTarget(); // create new, unresolved target
	LinkTarget_t NewTarget( void *pWhere );
	LinkTarget_t NewTargetHere(); // creates a target right here
	void SetTargetHere( LinkTarget_t ); // sets the given target to point to right here
	void SetTargetNull( LinkTarget_t ); // set this target to point to NULL

	// make the targets point to the same place
	//void Assign(LinkTarget_t assignee, LinkTarget_t assignor);

	bool Compile( void *pBuffer );
	uint GetTotalSize()const
	{
		return m_size;
	}

	bool IsDeclared( LinkTarget_t linkTarget )const;
	bool IsSet( LinkTarget_t linkTarget )const;
	bool IsDefined( LinkSource_t linkSource )const;
	bool IsLinked( LinkSource_t linkSource )const;

	const char* WriteAndLinkString( Offset_t<char> *pOffset, const char *pString );

	long Tell()const
	{
		return m_size;
	}

	void Begin( const char *nName, uint flags = 0 );
	void End();
	void PrintStats();
	void ClearStats();
protected:
	LinkSource_t NewLinkSource( int offsetOfOffset, OffsetTypeEnum type, int linkTargetId, const char *szDescription );
	int GetOffsetTo( const void *ptr )const;
	void SetTarget( LinkTarget_t linkTarget, int offsetToTarget );
protected:
	std::vector<Chunk> m_data;
	struct StringTableElement_t
	{
		char *m_pString;
		int m_nOffset;
		StringTableElement_t(): m_pString( NULL ), m_nOffset( NULL ) {}
	};
	CUtlStringMap<StringTableElement_t> m_stringTable; // each string offset

	uint m_size; // this is the currently used data
	int m_lastTarget; // the last id of LinkTarget

	uint m_alignBits; // the ( max alignment - 1 ) of the current block of data

	// LinkSource_t::m_offset -> ...
	LinkSourceMap m_mapSources;
	//
	LinkTargetMap m_mapTargets;

	struct BlockStats_t
	{
		BlockStats_t(): m_size( 0 ) {}
		long m_size;
	};
	typedef std::map<std::string, BlockStats_t>BlockStatsMap;
	BlockStatsMap m_mapBlockStats;
	struct Block_t
	{
		std::string m_name;
		long m_tell;
	};
	std::vector<Block_t> m_stackBlocks;
};




}

#endif
