//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HEGRENADE_PROJECTILE_H
#define HEGRENADE_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

#include "basecsgrenade_projectile.h"

class CHEGrenadeProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CHEGrenadeProjectile, CBaseCSGrenadeProjectile );

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

// Overrides.
public:
	CHEGrenadeProjectile() {}

	virtual void Spawn();
	virtual void Precache();
	virtual void BounceSound( void );
	virtual void Detonate();
	virtual const char *GetParticleSystemName( int pointContents, surfacedata_t *pdata );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

// Grenade stuff.
public:

	static CHEGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner, 
		const CCSWeaponInfo& weaponInfo,
		float timer );

	void SetTimer( float timer );

	void	InitializeSpawnFromWorld( inputdata_t &inputdata );

private:
	float m_flDetonateTime;
};


#endif // HEGRENADE_PROJECTILE_H
