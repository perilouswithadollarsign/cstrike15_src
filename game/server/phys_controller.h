//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PHYS_CONTROLLER_H
#define PHYS_CONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#define SF_THRUST_STARTACTIVE		0x0001
#define SF_THRUST_FORCE				0x0002
#define SF_THRUST_TORQUE			0x0004
#define SF_THRUST_LOCAL_ORIENTATION	0x0008
#define SF_THRUST_MASS_INDEPENDENT	0x0010
#define SF_THRUST_IGNORE_POS		0x0020

CBaseEntity *CreateKeepUpright( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flAngularLimit, bool bActive );
CBaseEntity *CreatePhysThruster( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, float flForce, float flForcetime, bool bActive, int nFlags );

AngularImpulse ComputeRotSpeedToAlignAxes( const Vector &testAxis, const Vector &alignAxis, const AngularImpulse &currentSpeed, 
										  float damping, float scale, float maxSpeed );

#endif // PHYS_CONTROLLER_H
