//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAP_H
#define MAP_H
#ifdef _WIN32
#pragma once
#endif


// All the brush sides referenced by info_no_dynamic_shadow entities.
extern CUtlVector<int> g_NoDynamicShadowSides;


class IMapDataFilesMgr
{
public:
	virtual void RegisterFile( char const *szFileName, CUtlBuffer &bufData ) = 0;
	virtual bool ReadRegisteredFile( char const *szFileName, CUtlBuffer &bufRead ) = 0;
	virtual void AddAllRegisteredFilesToPak() = 0;
};

IMapDataFilesMgr *GetMapDataFilesMgr();


#endif // MAP_H
