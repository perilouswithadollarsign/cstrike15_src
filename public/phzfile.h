//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: data structures that studiomdl writes to disk and vphysics.dll is able to map 
//          and use directly in memory without patching or processing.
//
// $NoKeywords: $
//=============================================================================//
#ifndef PHZFILE_H
#define PHZFILE_H

#include "datalinker_interface.h"

struct Physics2CollisionHeader_t
{
	int m_toolVersion; // this is the version of this structure
	int m_dataVersion; // this is the Serialize Version out of VPhysics

	DataLinker::Offset_t< void >  m_polytope; // an array of polytopes 
	DataLinker::Offset_t< void >  m_polysoup, m_mopp; // not used for now, but the extension is obvious

	DataLinker::Offset_t< void > m_inertia;

	int m_reserved;
};


#endif