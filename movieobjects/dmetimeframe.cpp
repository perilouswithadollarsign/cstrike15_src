//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmetimeframe.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTimeFrame, CDmeTimeFrame );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeTimeFrame::OnConstruction()
{
	m_Start   .InitAndSet( this, "start",    DMETIME_ZERO, FATTRIB_HAS_CALLBACK );
	m_Duration.InitAndSet( this, "duration", DMETIME_ZERO, FATTRIB_HAS_CALLBACK );
	m_Offset  .InitAndSet( this, "offset",   DMETIME_ZERO );
	m_Scale   .InitAndSet( this, "scale",    1.0f );
}

void CDmeTimeFrame::OnDestruction()
{
}


void CDmeTimeFrame::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	// notify parent clip that the time has changed
	if ( pAttribute == m_Start.GetAttribute() || pAttribute == m_Duration.GetAttribute() )
	{
		InvokeOnAttributeChangedOnReferrers( GetHandle(), pAttribute );
	}
}

void CDmeTimeFrame::SetEndTime( DmeTime_t endTime, bool bChangeDuration )
{
	if ( bChangeDuration )
	{
		m_Duration = endTime - m_Start;
	}
	else
	{
		m_Start = endTime - m_Duration;
	}
}

void CDmeTimeFrame::SetTimeScale( float flScale, DmeTime_t scaleCenter, bool bChangeDuration )
{
#ifdef _DEBUG
	DmeTime_t preCenterTime = ToChildMediaTime( scaleCenter, false );
#endif

	float ratio = m_Scale / flScale;
	DmeTime_t t = scaleCenter - m_Start;

	if ( bChangeDuration )
	{
		DmeTime_t newDuration = m_Duration.Get() * ratio;

		if ( scaleCenter != m_Start )
		{
			DmeTime_t newStart = ( m_Start.Get() - scaleCenter ) * ratio + scaleCenter;
			SetStartTime( newStart );
		}

		SetTimeOffset( ( t + m_Offset.Get() ) * ratio + m_Start.Get() - scaleCenter );
		SetDuration( newDuration );
	}
	else
	{
		SetTimeOffset( ( t + m_Offset.Get() ) * ratio - t );
	}

	SetTimeScale( flScale );

#ifdef _DEBUG
	DmeTime_t postCenterTime = ToChildMediaTime( scaleCenter, false );
	Assert( abs( preCenterTime - postCenterTime ) <= DMETIME_MINDELTA );
#endif
}
