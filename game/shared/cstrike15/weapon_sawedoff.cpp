//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "fx_cs_shared.h"

#if defined( CLIENT_DLL )

	#define CWeaponSawedoff C_WeaponSawedoff
	#include "c_cs_player.h"

#else

	#include "cs_player.h"
	#include "te_shotgun_shot.h"

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class CWeaponSawedoff : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CWeaponSawedoff, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponSawedoff( void );

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_SAWEDOFF; }

	virtual int GetShotgunReloadState( void ) { return m_reloadState; }

private:

	CWeaponSawedoff( const CWeaponSawedoff & );

	float m_flPumpTime;
	CNetworkVar( int, m_reloadState );

};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSawedoff, DT_WeaponSawedoff )

BEGIN_NETWORK_TABLE( CWeaponSawedoff, DT_WeaponSawedoff )
#if defined( CLIENT_DLL )
	RecvPropInt( RECVINFO( m_reloadState ) )
#else
	SendPropInt( SENDINFO( m_reloadState ), 2, SPROP_UNSIGNED )
#endif
END_NETWORK_TABLE()

#if defined( CLIENT_DLL )
	BEGIN_PREDICTION_DATA( CWeaponSawedoff )
	DEFINE_PRED_FIELD( m_reloadState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( weapon_sawedoff, WeaponSawedoff );
// PRECACHE_REGISTER( weapon_sawedoff );



CWeaponSawedoff::CWeaponSawedoff( void )
{
	m_flPumpTime = 0.0f;
	m_reloadState = 0;
}


void CWeaponSawedoff::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CCSWeaponInfo& weaponInfo = GetCSWpnData();

	float flCycleTime = GetCycleTime();

	// don't fire underwater
	if ( pPlayer->GetWaterLevel() == WL_Eyes )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.15f;
		return;
	}

	// Out of ammo?
	if ( m_iClip1 <= 0 )
	{
		Reload();
		if ( m_iClip1 == 0 )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		}

		return;
	}

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->DoMuzzleFlash();

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	uint16 nItemDefIndex = 0;

	// Dispatch the FX right away with full accuracy.
	float flCurAttack = CalculateNextAttackTime( flCycleTime );
	FX_FireBullets( 
		pPlayer->entindex(),
		nItemDefIndex,
		pPlayer->Weapon_ShootPosition(), 
		pPlayer->GetFinalAimAngle(),
		GetCSWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed( SERVER_PLATTIME_RNG ) & 255, // wrap it for network traffic so it's the same between client and server
		GetInaccuracy(),
		GetSpread(),
		GetAccuracyFishtail(),
		flCurAttack,
		SINGLE,
		m_flRecoilIndex );

	// are we firing the last round in the clip?
	if ( m_iClip1 == 1 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 0.875f );
	}
	else
	{
		m_flPumpTime = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 2.5f );
	}


	m_reloadState = 0;

	// update accuracy
	m_fAccuracyPenalty += weaponInfo.GetInaccuracyFire( GetEconItemView(), Primary_Mode );

	// table driven recoil
	Recoil( Primary_Mode );

	++pPlayer->m_iShotsFired;
	m_flRecoilIndex += 1.0f;
	--m_iClip1;
}


bool CWeaponSawedoff::Reload( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	if ( GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) <= 0 || m_iClip1 == GetMaxClip1() )
		return true;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return true;
		
	// check to see if we're ready to reload
	if ( m_reloadState == 0 )
	{
		pPlayer->SetAnimation( PLAYER_RELOAD );

		SendWeaponAnim( ACT_SHOTGUN_RELOAD_START );
		m_reloadState = 1;
		pPlayer->m_flNextAttack = gpGlobals->curtime + 0.5f;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );

#if defined( GAME_DLL )
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_START );
#endif

		return true;
	}
	else if ( m_reloadState == 1 )
	{
		if ( m_flTimeWeaponIdle > gpGlobals->curtime )
			return true;
		// was waiting for gun to move to side
		m_reloadState = 2;

		SendWeaponAnim( ACT_VM_RELOAD );
		SetWeaponIdleTime( gpGlobals->curtime + 0.5f );
#if defined( GAME_DLL )
		// [mlowrance] Only play the looping anim
		pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_LOOP );
#endif
	}
	else
	{
		// Add them to the clip
		m_iClip1 += 1;
		
#if defined( GAME_DLL )
		SendActivityEvents();
#endif
		
		GiveReserveAmmo( AMMO_POSITION_PRIMARY, -1, true );
		m_reloadState = 1;
	}

	return true;
}


void CWeaponSawedoff::WeaponIdle( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_flPumpTime && m_flPumpTime < gpGlobals->curtime )
	{
		// play pumping sound
		m_flPumpTime = 0.0f;
	}

	if ( m_flTimeWeaponIdle < gpGlobals->curtime )
	{
		if ( m_iClip1 == 0 && m_reloadState == 0 && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
		{
			Reload();
		}
		else if ( m_reloadState != 0 )
		{
			if ( m_iClip1 != GetMaxClip1() && GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) )
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				//MIKETODO: shotgun anims
				SendWeaponAnim( ACT_SHOTGUN_RELOAD_FINISH );
				
#if defined( GAME_DLL )
				// [mlowrance] play the finish for 3rd person
				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD_END );
#endif
				// play cocking sound
				m_reloadState = 0;
				SetWeaponIdleTime( gpGlobals->curtime + 1.5f );
			}
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}
