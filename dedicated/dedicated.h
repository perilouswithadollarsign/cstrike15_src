//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef DEDICATED_H
#define DEDICATED_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/tier3app.h"


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class IDedicatedServerAPI;


//-----------------------------------------------------------------------------
// Singleton interfaces 
//-----------------------------------------------------------------------------
extern IDedicatedServerAPI *engine;


//-----------------------------------------------------------------------------
// Inner loop: initialize, shutdown main systems, load steam to 
//-----------------------------------------------------------------------------
#ifdef POSIX
#define DEDICATED_BASECLASS CTier2SteamApp
#else
#define DEDICATED_BASECLASS CVguiSteamApp
#endif

class CDedicatedAppSystemGroup : public DEDICATED_BASECLASS
{
	typedef DEDICATED_BASECLASS BaseClass;

public:
	// Methods of IApplication
	virtual bool Create( );
	virtual bool PreInit( );
	virtual int Main( );
	virtual void PostShutdown();
	virtual void Destroy();

	// Used to chain to base class
	AppModule_t LoadModule( CreateInterfaceFn factory )
	{
		return CSteamAppSystemGroup::LoadModule( factory );
	}

	// Method to add various global singleton systems 
	bool AddSystems( AppSystemInfo_t *pSystems )
	{
		return CSteamAppSystemGroup::AddSystems( pSystems );
	}

	void *FindSystem( const char *pInterfaceName )
	{
		return CSteamAppSystemGroup::FindSystem( pInterfaceName );
	}
};


// main server entry point after init
void RunServer( bool bSupressStdIOBecauseWeAreAForkedChild );

#ifdef _LINUX
void RunServerSubProcesses( int nNumChildInstances );
#endif

// modify the command line, replacing all occurrences of ## with nIndex
void PerformCommandLineSubstitutions( int nIndex );


// forked subprocess index. 0 for master or not forked mode
extern int g_nSubProcessId;

#endif // DEDICATED_H
