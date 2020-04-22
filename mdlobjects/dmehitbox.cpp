//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a hitbox
//
//===========================================================================//


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmehitbox.h"
#include "tier2/renderutils.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeHitbox, CDmeHitbox );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeHitbox::OnConstruction()
{
	m_sSurfaceProperty.Init( this, "surfaceProperty" );
	m_nGroupId.Init( this, "groupId" );
	m_sBoneName.Init( this, "boneName" );
	m_cRenderColor.InitAndSet( this, "renderColor", Color( 255, 255, 255, 64 ) );

	// Set by CDmeBBox to FLT_MAX, -FLT_MAX
	m_vMinBounds = Vector( 0.0f, 0.0f, 0.0f );
	m_vMaxBounds = Vector( 0.0f, 0.0f, 0.0f );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeHitbox::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Rendering method for the dag
//-----------------------------------------------------------------------------
void CDmeHitbox::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	Vector vecOrigin;
	QAngle angles;
	MatrixAngles( shapeToWorld, angles, vecOrigin );
	RenderBox( vecOrigin, angles, m_vMinBounds, m_vMaxBounds, m_cRenderColor, true );
}