//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef STEAM_CONTENTSERVER_H
#define STEAM_CONTENTSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "steam_api.h"
#include "isteamcontentserver.h"

S_API bool SteamContentServer_Init( uint32 uContentServerID, uint32 unIP, uint16 usPort, uint16 usClientContentPort );
S_API void SteamContentServer_Shutdown();
S_API void SteamContentServer_RunCallbacks();

S_API ISteamContentServer *SteamContentServer();
S_API ISteamUtils *SteamContentServerUtils();

#define STEAM_CONTENTSERVER_CALLBACK( thisclass, func, param, var ) CCallback< thisclass, param, true > var; void func( param *pParam )


#endif // STEAM_GAMESERVER_H
