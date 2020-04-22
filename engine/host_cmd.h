//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( HOST_CMD_H )
#define HOST_CMD_H
#ifdef _WIN32
#pragma once
#endif

#include "savegame_version.h"
#include "host_saverestore.h"
#include "convar.h"

// The launcher includes this file, too
#ifndef LAUNCHERONLY

void Host_Init( bool bIsDedicated );
void Host_Shutdown(void);
int  Host_Frame (float time, int iState );
void Host_ShutdownServer(void);
bool Host_NewGame( char *mapName, char *mapGroupName, bool loadGame, bool bBackgroundLevel, bool bSplitScreenConnect, const char *pszOldMap = NULL, const char *pszLandmark = NULL );
void Host_Changelevel( bool loadfromsavedgame, const char *mapname, char *mapGroupName, const char *start );
void Host_PrintStatus( cmd_source_t commandSource, void ( *print )(const char *fmt, ...), bool bShort );

const char *GetHostProductString();
const char *GetHostVersionString();
int32 GetHostVersion();
int32 GetClientVersion();
int32 GetServerVersion();

extern ConVar host_name;

extern int  gHostSpawnCount;

#endif // LAUNCHERONLY
#endif // HOST_CMD_H
