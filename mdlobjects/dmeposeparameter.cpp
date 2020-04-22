//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme version of QC $poseparameter
//
//===========================================================================//


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeposeparameter.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePoseParameter, CDmePoseParameter );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePoseParameter::OnConstruction()
{
	m_flMin.Init( this, "min" );
	m_flMax.Init( this, "max" );
	m_bLoop.Init( this, "loop" );
	m_flLoopRange.Init( this, "loopRange" );
	m_bWrap.Init( this, "wrap" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePoseParameter::OnDestruction()
{
}