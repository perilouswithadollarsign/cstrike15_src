//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: System environment utility code
//
//=====================================================================================//

#ifndef ENVIRONMENT_UTILS_H
#define ENVIRONMENT_UTILS_H
#pragma once

#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

// Return system include paths (for a given platform & compiler)
extern bool GetSystemIncludePaths( CUtlVector<CUtlString> &paths, const char *pPlatform, const char *pCompiler );

#endif // ENVIRONMENT_UTILS_H
