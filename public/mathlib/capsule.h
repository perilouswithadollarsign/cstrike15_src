//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef MATHLIB_CAPSULE_HDR
#define MATHLIB_CAPSULE_HDR

#include "vector.h"
#include "rubikon/param_types.h"

void CastCapsuleRay( CShapeCastResult& out, const Vector& vRayStart, const Vector& vRayDelta, const Vector vCenter[], float flRadius );

#endif
