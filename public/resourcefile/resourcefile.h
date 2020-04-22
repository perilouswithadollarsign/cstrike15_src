//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Describes our resource format. All resource files have a simplistic
// dictionary of resource "blocks", which can be looked up using a block type id.
// Each resource block type is expected to be associated with a well defined
// structure. The macro DEFINE_RESOURCE_BLOCK_TYPE is used to create this
// association. 
//
// The current design choice is that we expect files using this resource format
// to be small. Large-scale files, like sound or texture bits, are expected to
// exist in a file parallel to the file containing these resource blocks owing
// to issues of alignment or streaming. We therefore expect users of files
// containing the data described in this header to load the entire file in
// so that all blocks are in memory.
//
// $NoKeywords: $
//===========================================================================//

#ifndef RESOURCEFILE_H
#define RESOURCEFILE_H
#pragma once

#include "resourcefile/schema.h"

#include "tier0/platform.h"
#include "resourcefile/resourcestream.h"
#include "datamap.h"


//-----------------------------------------------------------------------------
//
// On-disk structures related to the resource file format
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Resource block types
//-----------------------------------------------------------------------------
typedef uint32 ResourceBlockId_t;


//-----------------------------------------------------------------------------
// A block of resource data
//-----------------------------------------------------------------------------
struct ResourceBlockEntry_t
{
	DECLARE_BYTESWAP_DATADESC();

	ResourceBlockId_t m_nBlockType;
	CResourcePointer<void> m_pBlockData;
};


//-----------------------------------------------------------------------------
// Structure of resource file header
//-----------------------------------------------------------------------------
enum ResourceFileHeaderVersion_t
{
	RESOURCE_FILE_HEADER_VERSION = 1,
};

struct ResourceFileHeader_t
{
	DECLARE_BYTESWAP_DATADESC();

	uint32 m_nVersion;			// see ResourceFileHeaderVersion_t
	uint32 m_nSizeInBytes;		// Size in bytes of entire file
	CResourceArray< ResourceBlockEntry_t > m_ResourceBlocks;
};


//-----------------------------------------------------------------------------
//
// Methods used to define resource blocks/read them from files
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Resource block IDs
//-----------------------------------------------------------------------------
#define RSRC_BYTE_POS( byteVal, shft )	ResourceBlockId_t( uint32(uint8(byteVal)) << uint8(shft * 8) )
#if !defined( PLATFORM_X360 )
#define MK_RSRC_BLOCK_ID(a, b, c, d)	ResourceBlockId_t( RSRC_BYTE_POS(a, 0) | RSRC_BYTE_POS(b, 1) | RSRC_BYTE_POS(c, 2) | RSRC_BYTE_POS(d, 3) )
#else
#define MK_RSRC_BLOCK_ID(a, b, c, d)	ResourceBlockId_t( RSRC_BYTE_POS(a, 3) | RSRC_BYTE_POS(b, 2) | RSRC_BYTE_POS(c, 1) | RSRC_BYTE_POS(d, 0) )
#endif

#define RESOURCE_BLOCK_ID_INVALID 0xFFFFFFFF


//-----------------------------------------------------------------------------
// Helpers used to define resource block IDs + associated structs
//-----------------------------------------------------------------------------
template< class T >
struct ResourceBlockIdSelector_t
{
	enum { RESOURCE_BLOCK_ID = RESOURCE_BLOCK_ID_INVALID };
};

#define DEFINE_RESOURCE_BLOCK_TYPE( _class, _ida, _idb, _idc, _idd ) \
	template <> struct ResourceBlockIdSelector_t< _class > { enum { RESOURCE_BLOCK_ID = MK_RSRC_BLOCK_ID( _ida, _idb, _idc, _idd ) }; };


//-----------------------------------------------------------------------------
// Does this resource file contain a particular block?
//-----------------------------------------------------------------------------
bool Resource_IsBlockDefined( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id );


//-----------------------------------------------------------------------------
// Gets the data associated with a particular data block
//-----------------------------------------------------------------------------
const void *Resource_GetBlock( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id );
template< class T > inline const T* Resource_GetBlock( const ResourceFileHeader_t *pHeader )
{
	return (const T*)Resource_GetBlock( pHeader, ResourceBlockIdSelector_t< T >::RESOURCE_BLOCK_ID );
}


//-----------------------------------------------------------------------------
// Helper methods to write block information
//-----------------------------------------------------------------------------
inline ResourceFileHeader_t *Resource_AllocateHeader( CResourceStream *pStream, int nBlockCount )
{
	ResourceFileHeader_t *pHeader = pStream->Allocate< ResourceFileHeader_t >();
	pHeader->m_nVersion = RESOURCE_FILE_HEADER_VERSION;
	pHeader->m_ResourceBlocks = pStream->Allocate< ResourceBlockEntry_t >( nBlockCount );
	return pHeader;
}


//-----------------------------------------------------------------------------
// Helper methods to write block information
//-----------------------------------------------------------------------------
template< class T > inline T* Resource_AllocateBlock( CResourceStream *pStream, ResourceFileHeader_t *pHeader, int nBlockIndex )
{
	pHeader->m_ResourceBlocks[nBlockIndex].m_nBlockType = ResourceBlockIdSelector_t< T >::RESOURCE_BLOCK_ID;
	T *pBlockData = pStream->Allocate< T >( 1 );
	pHeader->m_ResourceBlocks[nBlockIndex].m_pBlockData = pBlockData;
	return pBlockData;
}

#endif // RESOURCEFILE_H
