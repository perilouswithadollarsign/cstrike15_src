//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Real-Time Hierarchical Profiling
//
// $NoKeywords: $
//===========================================================================//

#include "pch_tier0.h"

#include "tier0/memalloc.h"
#include "tier0/valve_off.h"
#include "tier0/threadtools.h"

#include "vtuneinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef VTUNE_ENABLED

class VTuneInterfaceImpl : public VTuneInterface
{
public:

	VTuneInterfaceImpl()
	{
		m_pFrameDomain = NULL;
	}

	virtual void Init();
	virtual void StartFrame();
	virtual void EndFrame();
	virtual __itt_event CreateEvent( const char *name );

private:
	
	__itt_domain* m_pFrameDomain;
	CThreadFastMutex m_eventCreateMutex;
};

VTuneInterfaceImpl g_VTuneInterface;
VTuneInterface *g_pVTuneInterface = &g_VTuneInterface;

/*******************************************************************************
*
*	VTuneInterfaceImpl
*
*******************************************************************************/

void VTuneInterfaceImpl::Init()
{
	if ( !m_pFrameDomain )
	{
		m_pFrameDomain = __itt_domain_create( "Main" );
		m_pFrameDomain->flags = 1;
	}	
}

void VTuneInterfaceImpl::StartFrame()
{
	if ( m_pFrameDomain == NULL )
	{
		Init();
	}

	__itt_frame_begin_v3( m_pFrameDomain, NULL);
}

void VTuneInterfaceImpl::EndFrame()
{
	__itt_frame_end_v3( m_pFrameDomain, NULL);
}

__itt_event VTuneInterfaceImpl::CreateEvent( const char *name )
{
	AUTO_LOCK_FM( m_eventCreateMutex );
	return __itt_event_create( name, strlen( name ) );
}

void VTuneAutoEvent::Start()
{
	__itt_event_start( m_event );
}

void VTuneAutoEvent::End()
{
	__itt_event_end( m_event );
}

#endif	// VTUNE_ENABLED


