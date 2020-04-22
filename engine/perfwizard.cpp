//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "perfwizard.h"
#include "tier0/platform.h"
#include "con_nprint.h"
#include "server.h"
#include "client.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "KeyValues.h"
#include "vstdlib/ICommandLine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
ClientFPSTracker g_ClientFPSTracker;

//--------------------------------------------------------------------------------------------------------------
ClientFPSTracker::ClientFPSTracker()
{
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
void ClientFPSTracker::Reset( void )
{
	m_validTime = 0.0f;	// not valid yet

	m_minNonServerFPS = -1.0f;
	m_maxNonServerFPS = -1.0f;

	m_nonServerFPSAverage = -1.0f;

	m_minAvgNonServerFPS = -1.0f;
	m_maxAvgNonServerFPS = -1.0f;
}

//--------------------------------------------------------------------------------------------------------------
bool ClientFPSTracker::IsValid( void ) const
{
	return m_maxAvgNonServerFPS > 0.0f;
}

//--------------------------------------------------------------------------------------------------------------
void ClientFPSTracker::NPrint( int line ) const
{
#ifndef DEDICATED
	Con_NPrintf ( line++, "Min non-server FPS: %.0f", m_minNonServerFPS );
	Con_NPrintf ( line++, "Max non-server FPS: %.0f", m_maxNonServerFPS );

	line++;
	Con_NPrintf ( line++, "Avg non-server FPS: %.0f", m_nonServerFPSAverage );
	Con_NPrintf ( line++, "Min avg non-server FPS: %.0f", m_minAvgNonServerFPS );
	Con_NPrintf ( line++, "Max avg non-server FPS: %.0f", m_maxAvgNonServerFPS );
#endif // !DEDICATED
}

//--------------------------------------------------------------------------------------------------------------
void ClientFPSTracker::WriteData( void ) const
{
	if ( IsValid() && !CommandLine()->FindParm( "-makereslists" ) )
	{
		KeyValues *data = new KeyValues( "perfdata" );
		data->SetFloat( "minClientFPS", m_minNonServerFPS );
		data->SetFloat( "maxClientFPS", m_maxNonServerFPS );
		data->SetFloat( "minAveragedClientFPS", m_minAvgNonServerFPS );
		data->SetFloat( "maxAveragedClientFPS", m_maxAvgNonServerFPS );
		data->SaveToFile( g_pFileSystem, "cfg/perfdata.vdf", "MOD" );
		data->deleteThis();
	}
	else
	{
		//g_pFileSystem->RemoveFile( "cfg/perfdata.vdf", "MOD" );
	}
}

//--------------------------------------------------------------------------------------------------------------
void ClientFPSTracker::MarkFrame( float fps, float input, float client, float server, float render, float sound, float cl_dll, float exec )
{
	if ( sv.IsDedicated() )	// client-side only, so skip if we're a dedicated server
		return;

	if ( !cl.IsActive() )	// we only care about the time that a client is in the game
	{
		m_validTime = 0.0f;
		return;
	}

	if ( m_validTime == 0.0f )
	{
		m_validTime = Plat_FloatTime() + 10.0f; // allow some settling down
		return;
	}
	else
	{
		if ( m_validTime > Plat_FloatTime() )
		{
			return; // don't track fps in the settling-down phase after loading
		}
	}

	// Construct a client FPS by using every time component but the server.  This helps on listenservers,
	// since the performance wizard won't be able to do anything about server perf anyway.
	double nonServerFrameTime = (input + client + render + sound + cl_dll + exec) / 1000.0f;
	double nonServerFps;

	if ( nonServerFrameTime < 0.0001 )
	{
		nonServerFps = 999.0;
	}
	else
	{
		nonServerFps = 1.0 / nonServerFrameTime;
	}

	// Track min/max instantaneous FPS
	if ( m_minNonServerFPS < 0.0f )
	{
		m_minNonServerFPS = m_maxNonServerFPS = nonServerFps;
	}
	else
	{
		if ( nonServerFps < m_minNonServerFPS ) m_minNonServerFPS = nonServerFps;
		if ( nonServerFps > m_maxNonServerFPS ) m_maxNonServerFPS = nonServerFps;
	}

	// Construct an average FPS (this isn't really an average, since it weights the current frame AverageFraction, the previous frame by AverageFraction*AverageFraction, etc)
	const float AverageFraction = 0.99f;
	if ( m_nonServerFPSAverage < 0.0f )
	{
		m_nonServerFPSAverage = nonServerFps;
	}

	m_nonServerFPSAverage = m_nonServerFPSAverage * AverageFraction + ( 1.0f - AverageFraction ) * nonServerFps;

	// Track min/max averaged FPS.  These are the useful values for making performance recommendations.
	if ( m_minAvgNonServerFPS < 0.0f )
	{
		m_minAvgNonServerFPS = m_maxAvgNonServerFPS = m_nonServerFPSAverage;
	}
	else
	{
		if ( m_nonServerFPSAverage < m_minAvgNonServerFPS ) m_minAvgNonServerFPS = m_nonServerFPSAverage;
		if ( m_nonServerFPSAverage > m_maxAvgNonServerFPS ) m_maxAvgNonServerFPS = m_nonServerFPSAverage;
	}

	// Debug printing for test purposes.
	//NPrint( 10 );
}

//--------------------------------------------------------------------------------------------------------------
