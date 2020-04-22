#ifndef RESOURCEEXTREFLIST_G_H
#define RESOURCEEXTREFLIST_G_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "resourcefile/schema.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct ResourceExtRefList_t;
struct ResourceExtRefTypeList_t;
struct ResourceExtRefInfo_t;


//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
schema enum ResourceExtRefistVersion_t
{
	RESOURCE_EXT_REF_LIST_VERSION = 1,// (explicit)
};


//-----------------------------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------------------------

//!	resourceBlockType = "RERL"
schema struct ResourceExtRefList_t
{
	uint32           m_nVersion;
	CResourceArray< ResourceExtRefTypeList_t > m_ResourceTypeList;
	CResourceArray< CResourcePointer< char > > m_FileNameList;
};

DEFINE_RESOURCE_BLOCK_TYPE( ResourceExtRefList_t, 'R', 'E', 'R', 'L' )

schema struct ResourceExtRefTypeList_t
{
	uint32           m_nResourceType;	// see ResourceType_t
	CResourceArray< ResourceExtRefInfo_t > m_Resources;
};

schema struct ResourceExtRefInfo_t
{
	uint32           m_nId;	// see ResourceId_t
	uint32           m_nFileNameIndex;	// index into ResourceExtRefList_t::m_pFileNameList
	CResourcePointer< char > m_pResourceName;
};


#endif // RESOURCEEXTREFLIST_G_H
