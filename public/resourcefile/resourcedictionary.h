//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Describes our resource format. Resource files consist of two files:
// The first is a resource descriptor file containing various blocks of
// arbitrary data, including a resource dictionary describing raw data which
// is stored in a second, parallel file.
//
// $NoKeywords: $
//===========================================================================//

#ifndef RESOURCEDICTIONARY_H
#define RESOURCEDICTIONARY_H
#pragma once

#include "resourcefile/schema.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct ResourceDictionary_t;
struct ResourceDictionaryGroup_t;
struct ResourceDescription_t;


//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------
schema typedef uint32 ResourceId_t;


//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
schema enum ResourceDictionaryVersion_t
{
	RESOURCE_DICT_VERSION = 1,// (explicit)
};

schema enum ResourceCompressionType_t
{
	RESOURCE_COMPRESSION_NONE = 0,
};

schema enum ResourceStandardIds_t
{
	RESOURCE_ID_INVALID = 0,
};


//-----------------------------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------------------------

//! resourceBlockType = "RESD"
schema struct ResourceDictionary_t
{
	uint32           m_nCompressionType;
	uint32           m_nUncompressedLength;
	uint32           m_nPageSize;
	uint32           m_nAlignment;
	CResourceArray< ResourceDictionaryGroup_t > m_ResourceGroups;
};

DEFINE_RESOURCE_BLOCK_TYPE( ResourceDictionary_t, 'R', 'E', 'S', 'D' )

schema struct ResourceDictionaryGroup_t
{
	uint32           m_nResourceType;	// see ResourceType_t
	uint16           m_nCompressionType;
	uint16           m_nFlags;
	CResourceArray< ResourceDescription_t > m_Resources;
};

schema struct ResourceDescription_t
{
	uint32           m_nId;
	uint32           m_nStartOffset;
	uint32           m_nSize;
	uint32           m_nUncompressedSize;
};

//-----------------------------------------------------------------------------
// Computes a resource id given a resource name
// NOTE: Most resource names are of the format:
//		<relative file name under content>::<subresource name>
//-----------------------------------------------------------------------------
ResourceId_t ComputeResourceIdHash( const char *pResourceName );
ResourceId_t ComputeResourceIdHash( const char *pFileName, const char *pSubResourceName );
void GenerateResourceName( const char *pFileName, const char *pSubResourceName, char *pResourceName, size_t pBufLen );
void GenerateResourceFileName( const char *pFileName, char *pResourceFileName, size_t nBufLen );


#endif // RESOURCEDICTIONARY_H
