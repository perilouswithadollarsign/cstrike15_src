//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a procedural texture
//
//=============================================================================
#include "movieobjects/dmecycle.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include <math.h>

//-----------------------------------------------------------------------------
// Consts
//-----------------------------------------------------------------------------
#define INT_FLOAT_SCALE		0.000001f
//#define USE_NEW_WAY

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCycle, CDmeCycle );

//-----------------------------------------------------------------------------
// Implementation
//-----------------------------------------------------------------------------
void CDmeCycle::OnConstruction()
{
	m_cycleRate.Init( this, "cycleRate" );
	m_prevCycle.Init( this, "prevCycle" );
	m_lastCycleResetTime.Init( this, "lastCycleResetTime" );
	m_lastCycleResetValue.Init( this, "lastCycleResetValue" );
}

void CDmeCycle::OnDestruction()
{
}

void CDmeCycle::SetCycleRate( float flCycleRate )
{
	m_cycleRate = flCycleRate;
}

void CDmeCycle::SetPrevCycle( float flPrevCycle )
{
	m_prevCycle = (int)(flPrevCycle / INT_FLOAT_SCALE);
}

void CDmeCycle::SetCycle( float flCycle, float flCurTime )
{
#ifdef USE_NEW_WAY
	float const flCycleDelta = fabs( flCycle - GetPrevCycle() );

	// NOTE: Overlays depend on this logic - record if cycle==0.  There may be a better way to
	// do this, but if an overlay is at 0 while it's dormant (ie its weight==0), we need to 
	// record at the moment its weight>0.
	bool bForceCycleRecord = flCycle == 0.0f;

	if ( bForceCycleRecord || flCycleDelta >= 0.1f )
	{
		// Store this time
		m_lastCycleResetTime = (int)(flCurTime / INT_FLOAT_SCALE);

		// For this method, m_cycle is only recorded when a reset of some kind has occurred
		m_lastCycleResetValue = (int)(flCycle / INT_FLOAT_SCALE);
	
		DevMsg("              resetting cycle at:  time=%f  cycle=%f\n", flCurTime, flCycle );
	}
#else
	m_lastCycleResetValue = (int)(flCycle / INT_FLOAT_SCALE);
#endif

	// Store as previous cycle
	SetPrevCycle( flCycle );
}

float CDmeCycle::GetCycleRate() const
{
	return m_cycleRate;
}

float CDmeCycle::GetPrevCycle() const
{
	return m_prevCycle * INT_FLOAT_SCALE;
}

float CDmeCycle::GetCycle( float flCurTime ) const
{
#ifdef USE_NEW_WAY
	float const dt = flCurTime - INT_FLOAT_SCALE * m_lastCycleResetTime;
	float const flCycle = INT_FLOAT_SCALE * m_lastCycleResetValue + GetCycleRate() * dt;		Assert( flCycle >= 0.0f && flCycle <= 2.0f );
	return fmod( flCycle, 1.0f );		// In case it's 1.0f
#else
	return INT_FLOAT_SCALE * m_lastCycleResetValue;
#endif
}
