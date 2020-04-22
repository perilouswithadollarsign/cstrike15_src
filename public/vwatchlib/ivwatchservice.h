//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifndef IVWATCHSERVICE_H
#define IVWATCHSERVICE_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/interface.h"


#define VWATCH_SERVICE_VERSION	"VWatchService001"


class IVWatchService
{
public:
	// Fire it up.
	// pFactoryFn is used to get the filesystem interface.
	// It'll run until you call Stop() to make it stop.
	// Stop() must be called from another thread.
	// If it returns false, it will only do so during startup and that indicates a startup problem.
	virtual bool Run( CreateInterfaceFn pFactoryFn ) = 0;
	
	// Note: This can be called from any thread. It will interrupt the service as soon as it can and
	// return after the service has exited from its Run() function.
	virtual void Stop() = 0;
};


#endif // IVWATCHSERVICE_H
