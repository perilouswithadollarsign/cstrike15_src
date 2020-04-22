//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_CSBASE_GUN_H
#define WEAPON_CSBASE_GUN_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_csbase.h"


// This is the base class for pistols and rifles.
#if defined( CLIENT_DLL )

	#define CWeaponCSBaseGun C_WeaponCSBaseGun

#else
#endif


class CWeaponCSBaseGun : public CWeaponCSBase
{
public:
	
	DECLARE_CLASS( CWeaponCSBaseGun, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponCSBaseGun();

	virtual void Spawn();
	virtual void Precache();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();
	virtual bool Reload();
	virtual void WeaponIdle();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void Drop( const Vector &vecVelocity );
	virtual bool Deploy();

	// Derived classes call this to fire a bullet.
	bool CSBaseGunFire( float flCycleTime, CSWeaponMode weaponMode );

	void BurstFireRemaining( void );

	// Usually plays the shot sound. Guns with silencers can play different sounds.
	virtual void DoFireEffects();
	virtual void ItemPostFrame();
	virtual void ItemBusyFrame( void );

	float GetFOVForAccuracy( void );

	void SetWeaponInfo( const CCSWeaponInfo* pWeaponInfo )	{ m_pWeaponInfo = pWeaponInfo; }

	// Get CS-specific weapon data.
	virtual CCSWeaponInfo const	&GetCSWpnData() const;
	virtual int GetCSZoomLevel() {return m_zoomLevel; }

	CNetworkVar( int, m_zoomLevel );

	virtual bool HasZoom( void );
	virtual bool IsZoomed( void ) const;

	virtual bool WeaponHasBurst( void ) const { return GetCSWpnData().HasBurstMode( GetEconItemView() ); }
	virtual bool IsInBurstMode() const;

	virtual bool IsFullAuto() const;

	virtual bool IsRevolver( void ) const { return GetCSWpnData().IsRevolver( GetEconItemView() ); }
	virtual bool DoesUnzoomAfterShot( void ) const { return GetCSWpnData().DoesUnzoomAfterShot( GetEconItemView() ); }


	virtual bool SendWeaponAnim( int iActivity );

	CNetworkVar( int, m_iBurstShotsRemaining );	
	float	m_fNextBurstShot;			// time to shoot the next bullet in burst fire mode

	virtual Activity GetDeployActivity( void );

#ifdef CLIENT_DLL
	virtual const char		*GetMuzzleFlashEffectName_1stPerson( void );
	virtual const char		*GetMuzzleFlashEffectName_3rdPerson( void );
	int m_iSilencerBodygroup;
#endif

	virtual const char		*GetWorldModel( void ) const;

private:

	const CCSWeaponInfo*	m_pWeaponInfo;

	CWeaponCSBaseGun( const CWeaponCSBaseGun & );

	int m_silencedModelIndex;
	bool m_inPrecache;



};


#endif // WEAPON_CSBASE_GUN_H
