//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines scripting system.
//
//===========================================================================//


#ifndef GAMEUISCRIPTSYSTEM_H
#define GAMEUISCRIPTSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "vscript/ivscript.h"

IScriptVM *GameUIScriptSystemCreate();
HSCRIPT GameUIScriptSystemCompile( IScriptVM *pScriptVM, const char *pszScriptName, bool bWarnMissing );
bool	GameUIScriptSystemRun( IScriptVM *pScriptVM, const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing );

#endif // GAMEUISCRIPTSYSTEM_H

