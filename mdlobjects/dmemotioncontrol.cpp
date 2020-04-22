//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Animation/Sequence events
//
//==========================================================================


// Valve includes
#include "mdlobjects/dmemotioncontrol.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "studio.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMotionControl, CDmeMotionControl );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMotionControl::OnConstruction()
{
	m_bX.Init( this, "X" );
	m_bY.Init( this, "Y" );
	m_bZ.Init( this, "Z" );
	m_bXR.Init( this, "XR" );
	m_bYR.Init( this, "YR" );
	m_bZR.Init( this, "ZR" );
	m_bLX.Init( this, "LX" );
	m_bLY.Init( this, "LY" );
	m_bLZ.Init( this, "LZ" );
	m_bLXR.Init( this, "LXR" );
	m_bLYR.Init( this, "LYR" );
	m_bLZR.Init( this, "LZR" );
	m_bLM.Init( this, "LM" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMotionControl::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMotionControl::SetStudioMotionControl( int nStudioMotionControl )
{
	m_bX = ( nStudioMotionControl & STUDIO_X ) != 0;
	m_bY = ( nStudioMotionControl & STUDIO_Y ) != 0;
	m_bZ = ( nStudioMotionControl & STUDIO_Z ) != 0;
	m_bXR = ( nStudioMotionControl & STUDIO_XR ) != 0;
	m_bYR = ( nStudioMotionControl & STUDIO_YR ) != 0;
	m_bZR = ( nStudioMotionControl & STUDIO_ZR ) != 0;
	m_bLX = ( nStudioMotionControl & STUDIO_LX ) != 0;
	m_bLY = ( nStudioMotionControl & STUDIO_LY ) != 0;
	m_bLZ = ( nStudioMotionControl & STUDIO_LZ ) != 0;
	m_bLXR = ( nStudioMotionControl & STUDIO_LXR ) != 0;
	m_bLYR = ( nStudioMotionControl & STUDIO_LYR ) != 0;
	m_bLZR = ( nStudioMotionControl & STUDIO_LZR ) != 0;
	m_bLM = ( nStudioMotionControl & STUDIO_LINEAR ) != 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeMotionControl::GetStudioMotionControl() const
{
	return ( m_bX ? STUDIO_X : 0 ) |
		( m_bY ? STUDIO_Y : 0 ) |
		( m_bZ ? STUDIO_Z : 0 ) |
		( m_bXR ? STUDIO_XR : 0 ) |
		( m_bYR ? STUDIO_YR : 0 ) |
		( m_bZR ? STUDIO_ZR : 0 ) |
		( m_bLX ? STUDIO_LX : 0 ) |
		( m_bLY ? STUDIO_LY : 0 ) |
		( m_bLZ ? STUDIO_LZ : 0 ) |
		( m_bLXR ? STUDIO_LXR : 0 ) |
		( m_bLYR ? STUDIO_LYR : 0 ) |
		( m_bLZR ? STUDIO_LZR : 0 ) |
		( m_bLM ? STUDIO_LINEAR : 0 );
}