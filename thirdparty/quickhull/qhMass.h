//--------------------------------------------------------------------------------------------------
/**
	@file		qhMass.h

	@author		Dirk Gregorius
	@version	0.1
	@date		03/12/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include "qhTypes.h"
#include "qhMath.h"


//--------------------------------------------------------------------------------------------------
// qhMass
//--------------------------------------------------------------------------------------------------
struct qhMass
	{
	qhMass( void );

	qhReal Weight;
	qhVector3 Center;
	qhMatrix3 Inertia;

	qhMass& operator+=( const qhMass& Other );

	void ShiftToOrigin( void );
	void ShiftToCenter( void );
	};


qhMass qhSphereMass( const qhVector3& Center, qhReal Radius, qhReal Density = qhReal( 1 ) );
qhMass qhCapsuleMass( const qhVector3& Center1, const qhVector3& Center2, qhReal Radius, qhReal Density = qhReal( 1 ) );