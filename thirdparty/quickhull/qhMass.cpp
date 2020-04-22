//--------------------------------------------------------------------------------------------------
// qhMass.cpp
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------
#include "qhMass.h"


//--------------------------------------------------------------------------------------------------
// Local utilities
//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhSteiner( qhReal Weight, const qhVector3& Center )
{
	// Usage: Io = Ic + Is and Ic = Io - Is
	qhReal Ixx =  Weight * ( Center.Y * Center.Y + Center.Z * Center.Z );
	qhReal Iyy =  Weight * ( Center.X * Center.X + Center.Z * Center.Z );
	qhReal Izz =  Weight * ( Center.X * Center.X + Center.Y * Center.Y );
	qhReal Ixy = -Weight * Center.X * Center.Y;
	qhReal Ixz = -Weight * Center.X * Center.Z;
	qhReal Iyz = -Weight * Center.Y * Center.Z;

	// Write
	qhMatrix3 Out;
	Out.C1.X = Ixx; Out.C2.X = Ixy; Out.C3.X = Ixz;
	Out.C1.Y = Ixy;	Out.C2.Y = Iyy;	Out.C3.Y = Iyz;
	Out.C1.Z = Ixz;	Out.C2.Z = Iyz;	Out.C3.Z = Izz;

	return Out;
}


//--------------------------------------------------------------------------------------------------
// qhMass
//--------------------------------------------------------------------------------------------------
qhMass::qhMass( void )
: Weight( 0 )
, Center( QH_VEC3_ZERO )
, Inertia( QH_MAT3_ZERO )
{

}


//--------------------------------------------------------------------------------------------------
qhMass& qhMass::operator+=( const qhMass& Other )
{
	Inertia += Other.Inertia;
	Center = ( Weight * Center + Other.Weight * Other.Center ) / ( Weight + Other.Weight );
	Weight += Other.Weight;

	return *this;
}


//--------------------------------------------------------------------------------------------------
void qhMass::ShiftToOrigin( void )
{
	Inertia += qhSteiner( Weight, Center );
}


//--------------------------------------------------------------------------------------------------
void qhMass::ShiftToCenter( void )
{
	Inertia -= qhSteiner( Weight, Center );
}


//--------------------------------------------------------------------------------------------------
qhMass qhSphereMass( const qhVector3& Center, qhReal Radius, qhReal Density )
{
	qhReal Volume = qhReal( 0.4 ) * QH_PI * Radius * Radius * Radius;
	qhReal Weight = Volume * Density;

	qhReal I = qhReal( 0.4 ) * Weight * Radius * Radius;

	qhMass Mass;
	Mass.Weight = Weight;
	Mass.Center = Center;
	Mass.Inertia = qhMatrix3( I, I, I ) + qhSteiner( Weight, Center );

	return Mass;
}


//--------------------------------------------------------------------------------------------------
qhMass qhCapsuleMass( const qhVector3& Center1, const qhVector3& Center2, qhReal Radius, qhReal Density )
{
	// Compute height and check if we degenerate into a sphere
	qhReal Height = qhDistance( Center1, Center2 );
	if ( Height < qhReal( 100 ) * QH_REAL_EPSILON )
	{
		return qhSphereMass( qhReal( 0.5 ) * ( Center1 + Center2 ), Radius, Density );
	}

	// Cylinder
	qhReal CylinderVolume = ( QH_PI * Radius * Radius ) * Height;
	qhReal CylinderWeight = CylinderVolume * Density;

	// Sphere
	qhReal SphereVolume = qhReal( 4.0 / 3.0 ) * QH_PI * Radius * Radius * Radius;
	qhReal SphereWeight = SphereVolume * Density;

	// Parallel Axis Theorem (Steiner)
	qhReal Offset = qhReal( 0.5 ) * Height + qhReal( 3.0 / 8.0 ) * Radius;

	qhReal Ix = qhReal( 1.0 / 12.0 ) * CylinderWeight * ( qhReal( 3 ) * Radius * Radius + Height * Height ) + qhReal( 83.0 / 320.0 ) * SphereWeight * Radius * Radius + SphereWeight * Offset * Offset;
	qhReal Iy = qhReal( 0.5 ) * CylinderWeight * Radius * Radius + qhReal( 0.4 ) * SphereWeight * Radius * Radius;

	// Align capsule axis with chosen up-axis
	qhReal Weight = SphereWeight + CylinderWeight;
	qhVector3 Center = qhReal( 0.5 ) * ( Center1 + Center2 );
	qhVector3 Direction = qhNormalize( Center2 - Center1 );
	qhQuaternion Q = qhRotation( QH_VEC3_AXIS_Y, Direction );
	qhMatrix3 R = qhConvert( Q );

	qhMass Mass;
	Mass.Weight = Weight;
	Mass.Center = Center;
	Mass.Inertia = R * qhMatrix3( Ix, Iy, Ix ) * qhTranspose( R ) + qhSteiner( Weight, Center );

	return Mass;
}