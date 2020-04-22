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

// engine interface
#include "steam/steam_api.h"
#include "cdll_client_int.h"
#include "tier2/tier2.h"
#include "matchmaking/imatchframework.h"

extern class IEngineVGui *enginevguifuncs;
#ifdef _GAMECONSOLE
extern class IXOnline  *xonline;
#endif

#endif // ENGINEINTERFACE_H
