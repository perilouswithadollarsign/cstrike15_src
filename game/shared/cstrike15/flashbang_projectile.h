//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FLASHBANG_PROJECTILE_H
#define FLASHBANG_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif


#include "basecsgrenade_projectile.h"


class CFlashbangProjectile : public CBaseCSGrenadeProjectile
{
public:
	DECLARE_CLASS( CFlashbangProjectile, CBaseCSGrenadeProjectile );

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

// Overrides.
public:
	CFlashbangProjectile();

	virtual void Spawn();
	virtual void Precache();
	virtual void BounceSound( void );
	virtual void Detonate();
	
	void	InputSetTimer( inputdata_t &inputdata );

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_FLASH; }

// Grenade stuff.
	static CFlashbangProjectile* Create( 
		const Vector &position, 
		const QAngle &angles, 
		const Vector &velocity, 
		const AngularImpulse &angVelocity, 
		CBaseCombatCharacter *pOwner,
		const CCSWeaponInfo& weaponInfo );	
	
public:
	float m_flTimeToDetonate;

	// Count of players effected by the flash
	uint8 m_numOpponentsHit; // note: opponents are considered to be anybody not on the flasher's team.
	uint8 m_numTeammatesHit;
};


#endif // FLASHBANG_PROJECTILE_H
