//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ENGINETOOLS_INT_H
#define ENGINETOOLS_INT_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "interfaces/interfaces.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IEngineTool;
class IEngineVGui;
class IServerTools;
class IClientTools;
class IFileSystem;
class IP4;
class IVDebugOverlay;
class IDmSerializers;
class IVModelInfoClient;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern IEngineTool	*enginetools;
extern IEngineVGui	*enginevgui;
extern IServerTools	*servertools;
extern IClientTools	*clienttools;
extern IVModelInfoClient *modelinfoclient;

#ifndef HAMMER_FILESYSTEM_DEFINED
extern IFileSystem	*g_pFileSystem;
#endif

DECLARE_TIER2_INTERFACE( IP4, p4 );
extern IVDebugOverlay *debugoverlay;
extern IDmSerializers *dmserializers;


#endif // ENGINETOOLS_INT_H
