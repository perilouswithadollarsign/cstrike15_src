//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// DmeJiggleBone
//
//=============================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmejigglebone.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// DmeJiggleBone
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeJiggleBone, CDmeJiggleBone );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJiggleBone::OnConstruction()
{
	// flags
	m_bRigid.InitAndSet( this, "rigid", false );
	m_bFlexible.InitAndSet( this, "flexible", false );
	m_bBaseSpring.InitAndSet( this, "baseSpring", false );
	m_bYawConstrained.InitAndSet( this, "yawConstrained", false );
	m_bPitchConstrained.InitAndSet( this, "pitchConstrained", false );
	m_bLengthConstrained.InitAndSet( this, "lengthConstrained", false );
	m_bAngleConstrained.InitAndSet( this, "angleConstrained", false );

	// general params
	m_flLength.InitAndSet( this, "length", 10.0f );
	m_flTipMass.Init( this, "tipMass" );

	// angle constraint
	m_flAngleLimit.Init( this, "angleLimit" );

	// yaw constraint
	m_flYawMin.Init( this, "yawMin" );
	m_flYawMax.Init( this, "yawMax" );
	m_flYawFriction.Init( this, "yawFriction" );
	m_flYawBounce.Init( this, "yawBounce" );

	// flexible params
	m_flYawStiffness.InitAndSet( this, "yawStiffness", 100.0f );
	m_flYawDamping.Init( this, "yawDamping" );

	m_flPitchStiffness.InitAndSet( this, "pitchStiffness", 100.0f );
	m_flPitchDamping.Init( this, "pitchDamping" );

	m_flAlongStiffness.InitAndSet( this, "alongStiffness", 100.0f );
	m_flAlongDamping.Init( this, "alongDamping" );

	// pitch constraint
	m_flPitchMin.Init( this, "pitchMin" );
	m_flPitchMax.Init( this, "pitchMax" );
	m_flPitchFriction.Init( this, "pitchFriction" );
	m_flPitchBounce.Init( this, "pitchBounce" );

	// base spring
	m_flBaseMass.Init( this, "baseMass" );
	m_flBaseStiffness.Init( this, "baseStiffness", 100.0f );
	m_flBaseDamping.Init( this, "baseDamping" );

	// base spring yaw
	m_flBaseYawMin.Init( this, "baseYawMin", -100.0f );
	m_flBaseYawMax.Init( this, "baseYawMax", 100.0f );
	m_flBaseYawFriction.Init( this, "baseYawFriction" );

	// base spring pitch
	m_flBasePitchMin.Init( this, "basePitchMin", -100.0f );
	m_flBasePitchMax.Init( this, "basePitchMax", 100.0f );
	m_flBasePitchFriction.Init( this, "basePitchFriction" );

	// base spring pitch
	m_flBaseAlongMin.Init( this, "baseAlongMin", -100.0f );
	m_flBaseAlongMax.Init( this, "baseAlongMax", 100.0f );
	m_flBaseAlongFriction.Init( this, "baseAlongFriction" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJiggleBone::OnDestruction()
{
}