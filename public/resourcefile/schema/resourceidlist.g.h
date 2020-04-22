#ifndef RESOURCEIDLIST_G_H
#define RESOURCEIDLIST_G_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "resourcefile/schema.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct ResourceIdList_t;
struct ResourceIdTypeList_t;
struct ResourceIdInfo_t;


//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
schema enum ResourceIdListVersion_t
{
	RESOURCE_ID_LIST_VERSION = 1,// (explicit)
};


//-----------------------------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------------------------
//! resourceBlockType = "RIDL"
schema struct ResourceIdList_t
{
	uint32           m_nVersion;
	CResourceArray< ResourceIdTypeList_t > m_ResourceTypeList;
};

DEFINE_RESOURCE_BLOCK_TYPE( ResourceIdList_t, 'R', 'I', 'D', 'L' )

schema struct ResourceIdTypeList_t
{
	uint32           m_nResourceType;	// see ResourceType_t
	CResourceArray< ResourceIdInfo_t > m_Resources;
};

schema struct ResourceIdInfo_t
{
	uint32           m_nId;	// see ResourceId_t
	uint32           m_nEstimatedSize;
	uint32           m_nPermanentDataSize;
	CResourcePointer< void > m_PermanentData;
};


#endif // RESOURCEIDLIST_G_H
