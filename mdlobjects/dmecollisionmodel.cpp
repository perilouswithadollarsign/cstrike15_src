//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a collisionmodel
//
//===========================================================================//


#include "mdlobjects/dmecollisionmodel.h"
#include "datamodel/dmelementfactoryhelper.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCollisionModel, CDmeCollisionModel );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCollisionModel::OnConstruction()
{
	m_flMass.InitAndSet( this, "mass", 0.0f );
	m_bAutomaticMassComputation.InitAndSet( this, "automaticMassComputation", true );
	m_flInertia.InitAndSet( this, "inertia", 1.0f );
	m_flDamping.InitAndSet( this, "damping", 0.0f );
	m_flRotationalDamping.InitAndSet( this, "rotationalDamping", 0.0f );
	m_flDrag.InitAndSet( this, "drag", -1.0f );
	m_nMaxConvexPieces.InitAndSet( this, "maxConvexPieces", 40 );
	m_bRemove2D.InitAndSet( this, "remove2d", false );
	m_flWeldPositionTolerance.InitAndSet( this, "weldPositionTolerance", 0.0f );
	m_flWeldNormalTolerance.InitAndSet( this, "weldNormalTolerance", 0.999f );
	m_bConcave.InitAndSet( this, "concave", false );
	m_bForceMassCenter.InitAndSet( this, "forceMassCenter", false );
	m_vecMassCenter.InitAndSet( this, "massCenter", Vector( 0.0f, 0.0f, 0.0f ) );
	m_bAssumeWorldSpace.InitAndSet( this, "assumeWorldSpace", false );
	m_SurfaceProperty.InitAndSet( this, "surfaceProperty", "default" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCollisionModel::OnDestruction()
{
}