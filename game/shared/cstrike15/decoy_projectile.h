//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DECOY_PROJECTILE_H
#define DECOY_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif


#include "basecsgrenade_projectile.h"


#if defined( CLIENT_DLL )

class C_DecoyProjectile : public C_BaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( C_DecoyProjectile, C_BaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();

	virtual bool Simulate( void );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

private:
	CUtlReference<CNewParticleEffect> m_decoyParticleEffect;

};

#else // GAME_DLL

struct DecoyWeaponProfile;

class CDecoyProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CDecoyProjectile, CBaseCSGrenadeProjectile );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

// Overrides.
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Detonate( void );
	virtual void BounceSound( void );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_DECOY; }

// Grenade stuff.
	static CDecoyProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );	

private:
	void Think_Detonate( void );
	void GunfireThink( void );
	void SetTimer( float timer );

	int m_shotsRemaining;
	float m_fExpireTime;
	DecoyWeaponProfile*	m_pProfile;
	CSWeaponID	m_decoyWeaponId;
	item_definition_index_t m_decoyWeaponDefIndex;
	WeaponSound_t m_decoyWeaponSoundType;
};

#endif // GAME_DLL

#endif // DECOY_PROJECTILE_H
