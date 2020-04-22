//========= Copyright (c) Valve Corporation, All rights reserved. ==========

#ifndef MATHLIB_BOX_BUOYANCY_H
#define MATHLIB_BOX_BUOYANCY_H

#include "ssemath.h"
#include "mathlib/vector4d.h"


// returns the volume of the part of the box submerged in water
// box is defined as mutually orthogonal half-sizes. THe halfsizes MUST be orthogonal!
// the water plane is z=0, and the box center's z coordinate is taken from the f4Origin parameter
// (only z is used in f4Origin)
extern fltx4 GetBoxBuoyancy4x3( const fltx4& f4a, const fltx4& f4b, const fltx4&f4c, const fltx4&f4Origin );

extern Vector4D GetBoxBuoyancy( const Vector& a, const Vector& b, const Vector& c, const Vector& vecOrigin );

// this takes a,b,c half-sizes and the center position of the box in the columns of the 3x4 matrix
extern fltx4 GetBoxBuoyancy3x4( const FourVectors &box );

extern void TestBuoyancy();


#endif
