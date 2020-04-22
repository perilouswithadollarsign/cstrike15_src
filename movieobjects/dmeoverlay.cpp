//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeoverlay.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "datamodel/dmattributevar.h"
#include "movieobjects/dmedag.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeOverlay, CDmeOverlay );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeOverlay::OnConstruction()
{
	m_flWeight.Init( this, "weight", FATTRIB_HAS_CALLBACK );
	m_nSequence.Init( this, "sequence", FATTRIB_HAS_CALLBACK );
	m_flPrevCycle.Init( this, "prevCycle", FATTRIB_HAS_CALLBACK );
	m_flCycle.Init( this, "cycle", FATTRIB_HAS_CALLBACK );
	m_nOrder.Init( this, "order", FATTRIB_HAS_CALLBACK );
	m_flPlaybackRate.Init( this, "playbackRate", FATTRIB_HAS_CALLBACK );
	m_flLayerAnimtime.Init( this, "layerAnimtime", FATTRIB_HAS_CALLBACK );
	m_flLayerFadeOuttime.Init( this, "layerFadeOuttime", FATTRIB_HAS_CALLBACK );
}

void CDmeOverlay::OnDestruction()
{
}

void CDmeOverlay::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	InvokeOnAttributeChangedOnReferrers( GetHandle(), pAttribute );
}

float CDmeOverlay::GetWeight() const
{
	return m_flWeight;
}

float CDmeOverlay::GetSequence() const
{
	return m_nSequence;
}

float CDmeOverlay::GetPrevCycle() const
{
	return m_flPrevCycle;
}

float CDmeOverlay::GetCycle() const
{
	return m_flCycle;
}

int CDmeOverlay::GetOrder() const
{
	return m_nOrder;
}

float CDmeOverlay::GetPlaybackRate() const
{
	return m_flPlaybackRate;
}

float CDmeOverlay::GetLayerAnimtime() const
{
	return m_flLayerAnimtime;
}

float CDmeOverlay::GetLayerFadeOuttime() const
{
	return m_flLayerFadeOuttime;
}

void CDmeOverlay::SetWeight( float flWeight )
{
	m_flWeight = flWeight;
}

void CDmeOverlay::SetSequence( int nSequence )
{
	m_nSequence = nSequence;
}

void CDmeOverlay::SetPrevCycle( float flPrevCycle )
{
	m_flPrevCycle = flPrevCycle;
}

void CDmeOverlay::SetCycle( float flCycle )
{
	m_flCycle = flCycle;
}

void CDmeOverlay::SetOrder( int nOrder )
{
	m_nOrder = nOrder;
}

void CDmeOverlay::SetPlaybackRate( float flPlaybackRate )
{
	m_flPlaybackRate = flPlaybackRate;
}

void CDmeOverlay::SetLayerAnimttime( float flAnimttime )
{
	m_flLayerAnimtime = flAnimttime;
}

void CDmeOverlay::SetLayerFadeOuttime( float flLayerFadeOuttime )
{
	m_flLayerFadeOuttime = flLayerFadeOuttime;
}

CDmAttribute *CDmeOverlay::GetWeightAttribute()
{
	return m_flWeight.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetSequenceAttribute()
{
	return m_nSequence.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetPrevCycleAttribute()
{
	return m_flPrevCycle.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetCycleAttribute()
{
	return m_flCycle.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetOrderAttribute()
{
	return m_nOrder.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetPlaybackRateAttribute()
{
	return m_flPlaybackRate.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetLayerAnimtimeAttribute()
{
	return m_flLayerAnimtime.GetAttribute();
}

CDmAttribute *CDmeOverlay::GetLayerFadeOuttimeAttribute()
{
	return m_flLayerFadeOuttime.GetAttribute();
}

CDmeDag *CDmeOverlay::GetDag()
{
	static CUtlSymbolLarge overlaySymbol = g_pDataModel->GetSymbol( "overlay" );
	CDmeDag *pDag = FindReferringElement< CDmeDag >( this, overlaySymbol );
	return pDag;
}
