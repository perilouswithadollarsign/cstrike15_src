//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef PERFWIZARD_H
#define PERFWIZARD_H

//--------------------------------------------------------------------------------------------------------------
/**
 * Tracks the non-server FPS, for performance monitoring.
 * This is used for a wizard to suggest video option fallbacks to improve performance.
 */
class ClientFPSTracker
{
public:
	ClientFPSTracker();
	void Reset( void );

	void WriteData( void ) const;

	void MarkFrame( float fps, float input, float client, float server, float render, float sound, float cl_dll, float exec );

	bool IsValid( void ) const;
	void NPrint( int line ) const;

private:
	float m_validTime;

	double m_minNonServerFPS;
	double m_maxNonServerFPS;

	double m_nonServerFPSAverage;

	double m_minAvgNonServerFPS;
	double m_maxAvgNonServerFPS;
};

extern ClientFPSTracker g_ClientFPSTracker;

//--------------------------------------------------------------------------------------------------------------

#endif // PERFWIZARD_H
