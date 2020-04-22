//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "buildrenderables_PS3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CPS3BuildRenderablesJob g_BuildRenderablesJob;
CPS3BuildRenderablesJob* g_pBuildRenderablesJob = &g_BuildRenderablesJob;

job_buildrenderables::JobDescriptor_t g_buildRenderablesJobDescriptor ALIGN128;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildRenderablesJob::Init( void )
{
	m_bEnabled = false;

	m_buildRenderablesJobData.EnsureCapacity(MAX_CONCURRENT_BUILDVIEWS);

	m_buildRenderablesJobCount = 0;

	// requires a SPURS instance, so register with VJobs
	if( g_pVJobs )
	{
		g_pVJobs->Register( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildRenderablesJob::Shutdown()
{
	g_pVJobs->Unregister( this ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildRenderablesJob::OnVjobsInit()
{
	m_bEnabled = true;

	g_buildRenderablesJobDescriptor.header = *m_pRoot->m_pJobBuildRenderables;

	g_buildRenderablesJobDescriptor.header.useInOutBuffer	= 1;
	g_buildRenderablesJobDescriptor.header.sizeStack		= (64*1024)/16;
	g_buildRenderablesJobDescriptor.header.sizeInOrInOut	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildRenderablesJob::OnVjobsShutdown()
{
	m_bEnabled = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
PS3BuildRenderablesJobData *CPS3BuildRenderablesJob::GetJobData( int job )
{
	return &m_buildRenderablesJobData[ job ];
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------



