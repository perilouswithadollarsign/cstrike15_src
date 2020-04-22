//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef MATHLIB_SPHERE_HDR
#define MATHLIB_SPHERE_HDR

#include "rubikon/param_types.h"
#include "vector.h"

void CastSphereRay( CShapeCastResult& out, const Vector &m, const Vector& p, const Vector& d, float flRadius );

#endif
