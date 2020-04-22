//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a procedural texture
//
//=============================================================================

#ifndef DMECYCLE_H
#define DMECYCLE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDag;

//-----------------------------------------------------------------------------
// A class representing a cycle, which can arbitrarily break C1 continuity.
// When these jumps occur, we need to make sure we interpolate properly.
//-----------------------------------------------------------------------------
class CDmeCycle : public CDmElement
{
	DEFINE_ELEMENT( CDmeCycle, CDmElement );

public:
	float GetCycle( float flCurTime ) const;
	float GetCycleRate() const;
	float GetPrevCycle() const;

	void SetCycle( float flCycle, float flCurTime );
	void SetCycleRate( float flCycleRate );

	// If transform is contained inside some kind of CDmeDag, return that (it's "parent")
	CDmeDag *GetDag();

private:
	void SetPrevCycle( float flPrevCycle );

	CDmaVar<float>		m_cycleRate;
	CDmaVar<int>		m_prevCycle;			// NOTE: Stored as an integers so they will not be interpolated
	CDmaVar<int>		m_lastCycleResetTime;
	CDmaVar<int>		m_lastCycleResetValue;
};


#endif // DMECYCLE_H
