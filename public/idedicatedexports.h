//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IDEDICATEDEXPORTS_H
#define IDEDICATEDEXPORTS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "appframework/iappsystem.h"


abstract_class IDedicatedExports : public IAppSystem
{
public:
	virtual void Sys_Printf( char *text ) = 0;
	virtual void RunServer() = 0;
	virtual bool IsGuiDedicatedServer() = 0;
};

#define VENGINE_DEDICATEDEXPORTS_API_VERSION "VENGINE_DEDICATEDEXPORTS_API_VERSION003"


#endif // IDEDICATEDEXPORTS_H
