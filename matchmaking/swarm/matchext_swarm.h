//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MATCHEXT_SWARM_H
#define MATCHEXT_SWARM_H
#ifdef _WIN32
#pragma once
#endif

#include "../mm_framework.h"
#include "matchmaking/swarm/imatchext_swarm.h"

class CMatchExtSwarm : public IMatchExtSwarm
{
	// Methods of IMatchExtSwarm
public:
	// Get server map information for the session settings
	virtual KeyValues * GetAllMissions();
	virtual KeyValues * GetMapInfo( KeyValues *pSettings, KeyValues **ppMissionInfo = NULL );
	virtual KeyValues * GetMapInfoByBspName( KeyValues *pSettings, char const *szBspMapName, KeyValues **ppMissionInfo = NULL );

public:
	void Initialize();
	void DebugPrint();
	
	void ParseMissionFromFile( char const *szFile, bool bBuiltIn );
	void MakeGameModeCopy( char const *szGameMode, char const *szCopyName );

public:
	CMatchExtSwarm();
	~CMatchExtSwarm();

private:
	KeyValues *m_pKeyValues;
	CUtlStringMap< KeyValues * > m_mapFilesLoaded;
	CUtlStringMap< KeyValues * > m_mapMissionsLoaded;
};

// Match extension singleton
extern CMatchExtSwarm *g_pMatchExtSwarm;

#endif // MATCHEXT_L4D_H
