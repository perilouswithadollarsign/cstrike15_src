//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a transform
//
//=============================================================================

#ifndef DMEOVERLAY_H
#define DMEOVERLAY_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct matrix3x4_t;
class CDmeDag;

//-----------------------------------------------------------------------------
// A class representing a transformation matrix
//-----------------------------------------------------------------------------
class CDmeOverlay : public CDmElement
{
	DEFINE_ELEMENT( CDmeOverlay, CDmElement );

public:
	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

	float GetWeight() const;
	float GetSequence() const;
	float GetPrevCycle() const;
	float GetCycle() const;
	int   GetOrder() const;
	float GetPlaybackRate() const;
	float GetLayerAnimtime() const;
	float GetLayerFadeOuttime() const;

	void SetWeight( float flWeight );
	void SetSequence( int nSequence );
	void SetPrevCycle( float flPrevCycle );
	void SetCycle( float flCycle );
	void SetOrder( int nOrder );
	void SetPlaybackRate( float flPlaybackRate );
	void SetLayerAnimttime( float flAnimttime );
	void SetLayerFadeOuttime( float flLayerFadeOuttime );

	CDmAttribute *GetWeightAttribute();
	CDmAttribute *GetSequenceAttribute();
	CDmAttribute *GetPrevCycleAttribute();
	CDmAttribute *GetCycleAttribute();
	CDmAttribute *GetOrderAttribute();
	CDmAttribute *GetPlaybackRateAttribute();
	CDmAttribute *GetLayerAnimtimeAttribute();
	CDmAttribute *GetLayerFadeOuttimeAttribute();

	// If transform is contained inside some kind of CDmeDag, return that (it's "parent")
	CDmeDag *GetDag();

private:
	CDmaVar<float>		m_flWeight;
	CDmaVar<int>		m_nSequence;
	CDmaVar<float>		m_flPrevCycle;
	CDmaVar<float>		m_flCycle;
	CDmaVar<int>		m_nOrder;
	CDmaVar<float>		m_flPlaybackRate;
	CDmaVar<float>		m_flLayerAnimtime;
	CDmaVar<float>		m_flLayerFadeOuttime;
};


#endif // DMEOVERLAY_H
