//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef SINGLEPLAYERSHAREDMEMORY_H
#define SINGLEPLAYERSHAREDMEMORY_H

#if defined( _WIN32 )
#pragma once
#endif

#include "basetypes.h"
#include "ispsharedmemory.h"
#include "tier1/utlvector.h"

class CSPSharedMemory;

class CSPSharedMemoryManager
{
public:
	ISPSharedMemory *GetSharedMemory( const char *handle, int ent_num );
	~CSPSharedMemoryManager( void );

private:
	CUtlVector<CSPSharedMemory *> m_SharedSpaces;
	friend class CSPSharedMemory;
};

extern CSPSharedMemoryManager *g_pSinglePlayerSharedMemoryManager;

#endif // SINGLEPLAYERSHAREDMEMORY_H


