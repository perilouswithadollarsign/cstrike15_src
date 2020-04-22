//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

// HDRFIXME: reduce the number of include files here.

#include "buildindices_PS3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CPS3BuildIndicesJob g_BuildIndicesJob;
CPS3BuildIndicesJob* g_pBuildIndicesJob = &g_BuildIndicesJob;

job_buildindices::JobDescriptor_t g_buildIndicesJobDescriptor ALIGN128;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildIndicesJob::Init( void )
{
	m_bEnabled = false;

	m_buildIndicesJobData.EnsureCapacity(256);

	m_buildIndicesJobCount = 0;

	// requires a SPURS instance, so register with VJobs
	if( g_pVJobs )
	{
		g_pVJobs->Register( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildIndicesJob::Shutdown()
{
	g_pVJobs->Unregister( this ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildIndicesJob::OnVjobsInit()
{
	m_bEnabled = true;

	g_buildIndicesJobDescriptor.header = *m_pRoot->m_pJobBuildIndices;

	g_buildIndicesJobDescriptor.header.useInOutBuffer	= 1;
	g_buildIndicesJobDescriptor.header.sizeStack		= (32*1024)/8;
	g_buildIndicesJobDescriptor.header.sizeInOrInOut	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildIndicesJob::OnVjobsShutdown()
{
	m_bEnabled = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildIndicesJob::Push( job_buildindices::JobDescriptor_t *pJobDescriptor )
{
	CELL_VERIFY( m_pRoot->m_queuePortBuildIndices.pushJob( &pJobDescriptor->header, sizeof(*pJobDescriptor), 0, CELL_SPURS_JOBQUEUE_FLAG_SYNC_JOB ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPS3BuildIndicesJob::Sync( void )
{
	CELL_VERIFY( m_pRoot->m_queuePortBuildIndices.sync( 0 ) );

	// reset job count
	m_buildIndicesJobCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
PS3BuildIndicesJobData *CPS3BuildIndicesJob::GetJobData( void )
{
 	if( m_buildIndicesJobCount > 255 )
 	{
 		m_buildIndicesJobCount = 0;
 		g_pBuildIndicesJob->Sync();
 	}

	return &m_buildIndicesJobData[ m_buildIndicesJobCount++ ];
}




//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------



