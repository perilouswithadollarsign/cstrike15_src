//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SENSORGRENADE_PROJECTILE_H
#define SENSORGRENADE_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif


#include "basecsgrenade_projectile.h"
#if defined( CLIENT_DLL )
#include "c_props.h"
#else // GAME_DLL
#include "props.h"
#endif

#if defined( CLIENT_DLL )

class C_SensorGrenadeProjectile : public C_BaseCSGrenadeProjectile//, public C_BreakableProp
{
public:
	DECLARE_CLASS( C_SensorGrenadeProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

private:
	CUtlReference<CNewParticleEffect> m_sensorgrenadeParticleEffect;

};

#else // GAME_DLL

struct SensorGrenadeWeaponProfile;

class CSensorGrenadeProjectile : public CBaseCSGrenadeProjectile//, public CBreakableProp
{
public:
	DECLARE_CLASS( CSensorGrenadeProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

// Overrides.
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Detonate( void );
	virtual void BounceTouch( CBaseEntity *other );
	virtual void BounceSound( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_SENSOR; }

// Grenade stuff.
	static CSensorGrenadeProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );	

private:
	void Think_Arm( void );
	void Think_Remove( void );
	void SensorThink( void );
	void SetTimer( float timer );
	void DoDetectWave( void );

	float m_fExpireTime;
	float m_fNextDetectPlayerSound;

	EHANDLE m_hDisplayGrenade;
};

#endif // GAME_DLL

#endif // SENSORGRENADE_PROJECTILE_H
