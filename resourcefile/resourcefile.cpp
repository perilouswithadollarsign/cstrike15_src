//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "resourcefile/resourcefile.h"
#include "tier0/dbg.h"

// Must be last
#include "tier0/memdbgon.h"

#ifdef OSX
#pragma GCC diagnostic ignored "-Wbool-conversions"			
#endif

//-----------------------------------------------------------------------------
// Does this resource file contain a particular block?
//-----------------------------------------------------------------------------
bool Resource_IsBlockDefined( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id )
{
	Assert( pHeader->m_nVersion == RESOURCE_FILE_HEADER_VERSION );
	if ( pHeader->m_nVersion != RESOURCE_FILE_HEADER_VERSION )
		return false;

	for ( int i = 0; i < pHeader->m_ResourceBlocks.Count(); ++i )
	{
		const ResourceBlockEntry_t &block = pHeader->m_ResourceBlocks[i];
		if ( block.m_nBlockType == id )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Gets the data associated with a particular data block
//-----------------------------------------------------------------------------
const void *Resource_GetBlock( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id )
{
	Assert( pHeader->m_nVersion == RESOURCE_FILE_HEADER_VERSION );
	if ( pHeader->m_nVersion != RESOURCE_FILE_HEADER_VERSION )
		return NULL;

	for ( int i = 0; i < pHeader->m_ResourceBlocks.Count(); ++i )
	{
		const ResourceBlockEntry_t &block = pHeader->m_ResourceBlocks[i];
		if ( block.m_nBlockType == id )
			return block.m_pBlockData;
	}
	return NULL;
}
