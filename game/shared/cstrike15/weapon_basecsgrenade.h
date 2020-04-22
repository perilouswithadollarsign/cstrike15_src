//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_BASECSGRENADE_H
#define WEAPON_BASECSGRENADE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_csbase.h"
#include "cs_shareddefs.h"

#ifdef CLIENT_DLL
	
	#define CBaseCSGrenade C_BaseCSGrenade

#endif

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
#define GRENADE_UNDERHAND_THRESHOLD 0.33f
#endif

class CBaseCSGrenade : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CBaseCSGrenade, CWeaponCSBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CBaseCSGrenade();

	virtual void	Precache();

	bool			Deploy();
	bool			Holster( CBaseCombatWeapon *pSwitchingTo );

	void			PrimaryAttack();
	void			SecondaryAttack();

	bool			Reload();

	virtual void	ItemPostFrame();
	virtual void	ItemPreFrame();
	
	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	void			DecrementAmmo( CBaseCombatCharacter *pOwner );
	virtual void	StartGrenadeThrow();
	virtual void	ThrowGrenade();
	virtual void	DropGrenade();

	bool IsPinPulled() const;
	bool IsBeingThrown() const { return m_fThrowTime > 0; }

	bool IsLoopingSoundPlaying( void ) { return m_bLoopingSoundPlaying; }
	void SetLoopingSoundPlaying( bool bPlaying ) { m_bLoopingSoundPlaying = bPlaying; }

	// true if grenade has been thrown or dropped
	bool			GetIsThrown( void ) { return !m_bIsHeldByPlayer; }

#ifdef CLIENT_DLL
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance ) OVERRIDE;
#endif

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	bool			IsThrownUnderhand( void ) { return (m_flThrowStrength <= GRENADE_UNDERHAND_THRESHOLD); }
	float			GetThrownStrength( void ) { return m_flThrowStrength; }
	float			ApproachThrownStrength( void );
#endif

#ifndef CLIENT_DLL
	bool HasEmittedProjectile( void ) const { return m_bHasEmittedProjectile; }
	bool m_bHasEmittedProjectile;
#endif

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	virtual bool AllowsAutoSwitchFrom( void ) const;

	int		CapabilitiesGet();
	
	// Each derived grenade class implements this.
	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
#endif

protected:
	CNetworkVar( bool, m_bRedraw );	// Draw the weapon again after throwing a grenade
	CNetworkVar( bool, m_bIsHeldByPlayer );	// is true when held by player, false when it's been thrown or dropped
	CNetworkVar( bool, m_bPinPulled );	// Set to true when the pin has been pulled but the grenade hasn't been thrown yet.
	CNetworkVar( float, m_fThrowTime ); // the time at which the grenade will be thrown.  If this value is 0 then the time hasn't been set yet.

	CNetworkVar( bool, m_bLoopingSoundPlaying );	// Set to true when the grenade is playing a looping sound
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	CNetworkVar( float, m_flThrowStrength );
#endif
private:
	CBaseCSGrenade( const CBaseCSGrenade & ) {}

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	float m_flThrowStrengthClientSmooth;
#endif
};


inline bool CBaseCSGrenade::IsPinPulled() const
{
	return m_bPinPulled;
}


#endif // WEAPON_BASECSGRENADE_H
