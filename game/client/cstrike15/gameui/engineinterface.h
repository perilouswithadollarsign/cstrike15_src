//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Includes all the headers/declarations necessary to access the
//			engine interface
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENGINEINTERFACE_H
#define ENGINEINTERFACE_H

#ifdef _WIN32
#pragma once
#endif

// these stupid set of includes are required to use the cdll_int interface
#include "mathlib/vector.h"
//#include "wrect.h"
#define IN_BUTTONS_H

// engine interface
#include "cdll_int.h"
#include "eiface.h"
#include "icvar.h"
#include "tier2/tier2.h"
#include "matchmaking/imatchframework.h"

#ifdef SWARM_DLL
#include "matchmaking/swarm/imatchext_swarm.h"
extern class IMatchExtSwarm *g_pMatchExtSwarm;
#endif

#ifdef PORTAL2_UITEST_DLL
class IMatchExtPortal2
{
public:
	inline KeyValues * GetAllMissions() { return NULL; }
	inline KeyValues * GetMapInfoByBspName( KeyValues *, char const *, KeyValues ** = NULL ) { return NULL; }
	inline KeyValues * GetMapInfo( KeyValues *, KeyValues ** = NULL ) { return NULL; }
};
extern class IMatchExtPortal2 *g_pMatchExtPortal2;
#endif

// engine interface singleton accessors
extern IVEngineClient *engine;
extern class IBik *bik;
extern class IEngineVGui *enginevguifuncs;
extern class IGameUIFuncs *gameuifuncs;
extern class IEngineSound *enginesound;
extern class IXboxSystem  *xboxsystem;
#ifdef _GAMECONSOLE
extern class IXOnline  *xonline;
#endif
extern class IAchievementMgr *achievementmgr; 
extern class CSteamAPIContext *steamapicontext;
#ifdef _PS3
#include "ps3/saverestore_ps3_api_ui.h"
extern class IPS3SaveRestoreToUI *ps3saveuiapi;
#endif

#endif // ENGINEINTERFACE_H
