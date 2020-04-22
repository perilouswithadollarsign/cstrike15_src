//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// DmeJiggleBone
//
//============================================================================


#ifndef DMEJIGGLEBONE_H
#define DMEJIGGLEBONE_H


#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "mdlobjects/dmeproceduralbone.h"


//-----------------------------------------------------------------------------
// DmeJiggleBone
//-----------------------------------------------------------------------------
class CDmeJiggleBone : public CDmeProceduralBone
{
	DEFINE_ELEMENT( CDmeJiggleBone, CDmeProceduralBone );

public:

	// flags
	CDmaVar< bool >		m_bRigid;
	CDmaVar< bool >		m_bFlexible;
	CDmaVar< bool >		m_bBaseSpring;
	CDmaVar< bool >		m_bYawConstrained;
	CDmaVar< bool >		m_bPitchConstrained;
	CDmaVar< bool >		m_bLengthConstrained;
	CDmaVar< bool >		m_bAngleConstrained;

	// general params
	CDmaVar< float >	m_flLength;	
	CDmaVar< float >	m_flTipMass;

	// angle constraint
	CDmaVar< float >	m_flAngleLimit;		// Angles

	// yaw constraint
	CDmaVar< float >	m_flYawMin;			// Angle
	CDmaVar< float >	m_flYawMax;			// Angle
	CDmaVar< float >	m_flYawFriction;
	CDmaVar< float >	m_flYawBounce;

	// pitch constraint
	CDmaVar< float >	m_flPitchMin;		// Angle
	CDmaVar< float >	m_flPitchMax;		// Angle
	CDmaVar< float >	m_flPitchFriction;
	CDmaVar< float >	m_flPitchBounce;

	// flexible params
	CDmaVar< float >	m_flYawStiffness;	// [0, 1000]
	CDmaVar< float >	m_flYawDamping;		// [0, 10]

	CDmaVar< float >	m_flPitchStiffness;	// [0, 1000]
	CDmaVar< float >	m_flPitchDamping;	// [0, 10]

	CDmaVar< float >	m_flAlongStiffness;	// [0, 1000]
	CDmaVar< float >	m_flAlongDamping;	// [0, 10]

	// base spring
	CDmaVar< float >	m_flBaseMass;
	CDmaVar< float >	m_flBaseStiffness;	// [0, 1000]
	CDmaVar< float >	m_flBaseDamping;	// [0, 10]

	// base spring yaw
	CDmaVar< float >	m_flBaseYawMin;
	CDmaVar< float >	m_flBaseYawMax;
	CDmaVar< float >	m_flBaseYawFriction;

	// base spring pitch
	CDmaVar< float >	m_flBasePitchMin;
	CDmaVar< float >	m_flBasePitchMax;
	CDmaVar< float >	m_flBasePitchFriction;

	// base spring along
	CDmaVar< float >	m_flBaseAlongMin;
	CDmaVar< float >	m_flBaseAlongMax;
	CDmaVar< float >	m_flBaseAlongFriction;

};


#endif // DMEJIGGLEBONE_H