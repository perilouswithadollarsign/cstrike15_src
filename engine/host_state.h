//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HOST_STATE_H
#define HOST_STATE_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

void		HostState_Init();
void		HostState_RunGameInit();
void		HostState_Frame( float time );
void		HostState_NewGame( char const *pMapName, bool remember_location, bool background, bool bSplitScreenConnect );
void		HostState_LoadGame( char const *pSaveFileName, bool remember_location, bool bLetToolsOverrideLoadGameEnts );
void		HostState_ChangeLevelSP( char const *pNewLevel, char const *pLandmarkName );
void		HostState_ChangeLevelMP( char const *pNewLevel, char const *pLandmarkName );
void		HostState_SetMapGroupName( char const *pMapGroupName );
void		HostState_GameShutdown();
void		HostState_Shutdown();
void		HostState_Restart();
bool		HostState_IsGameShuttingDown();
void		HostState_OnClientConnected();
void		HostState_OnClientDisconnected();
void		HostState_SetSpawnPoint(Vector &position, QAngle &angle);
bool		HostState_IsTransitioningToLoad();
bool		HostState_GameHasShutDownAndFlushedMemory();
void		HostState_Pre_LoadMapIntoMemory();
void		HostState_Post_FlushMapFromMemory();
const char	*HostState_GetNewLevel( void );

#endif // HOST_STATE_H
