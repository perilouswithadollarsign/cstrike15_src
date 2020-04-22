//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Real-Time Hierarchical Profiling
//
// $NoKeywords: $
//=============================================================================//

#ifndef VTUNEINTERFACE_H
#define VTUNEINTERFACE_H

//#define VTUNE_ENABLED

#ifdef VTUNE_ENABLED

#include "platform.h"
#include "..\thirdparty\vtune\include\ittnotify.h"

class VTuneInterface
{
public:

	virtual void Init() = 0;

	virtual void StartFrame() = 0;
	virtual void EndFrame() = 0;

	virtual __itt_event CreateEvent( const char *name ) = 0;
};

// VTuneEvent implements user events. By default starts when created, stops
// when out of scope. To change start behaviour set bStart = false in constructor 
// and call Start(). To change stop behaviour call Stop(). 

class VTuneAutoEvent
{
public:

	VTuneAutoEvent( __itt_event vtuneEvent )
	{
		m_event = vtuneEvent;
		Start();
	}

	~VTuneAutoEvent()
	{
		End();
	}

	PLATFORM_CLASS void Start();
	PLATFORM_CLASS void End();

private:

	__itt_event m_event;
};

PLATFORM_INTERFACE VTuneInterface *g_pVTuneInterface;

#define VTUNE_AUTO_EVENT( name )									\
	static __itt_event event_ ## name = 0;							\
	if ( ! (event_ ## name) )										\
	{																\
		event_ ## name = g_pVTuneInterface->CreateEvent( #name );	\
	}																\
	VTuneAutoEvent autoEvent_ ## name ( event_ ## name );	

#endif
	
#endif	// VTUNEINTERFACE_H

