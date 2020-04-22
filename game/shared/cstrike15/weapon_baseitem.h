//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: base class for belt items, eg pills and adrenaline
//
// $NoKeywords: $
//=====================================================================================//

#ifndef _WEAPON_BASE_ITEM_H_
#define _WEAPON_BASE_ITEM_H_

#ifdef _WIN32
#pragma once
#endif

#include "weapon_csbase.h"
#include "util_shared.h"

#if defined( CLIENT_DLL )
#define CWeaponBaseItem C_WeaponBaseItem
#endif

class CWeaponBaseItem : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CWeaponBaseItem, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

	CWeaponBaseItem();

	virtual bool HasPrimaryAmmo( void );				// Returns true if weapon has ammo
	virtual bool CanBeSelected( void );

	virtual void Spawn( void );

	virtual void PrimaryAttack( void );					// do "+ATTACK"
	virtual void SecondaryAttack( void );				// do "+ATTACK2"
//	virtual bool OnHit( trace_t &trace, const Vector &swingVector, bool firstTime );	// deals damage and plays hit effects.  returns true if this hit stops the swing.

	bool			Reload();

	virtual void ItemPostFrame( void );					// called each frame by the player PostThink
//	virtual bool CanExtendHelpingHand( void ) const;
	virtual bool CanFidget( void );

	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	virtual void WeaponIdle( void );					// called when no buttons pressed

	virtual bool SendWeaponAnim( int iActivity );
//	virtual Activity GetWeaponDeployActivity( PlayerAnimEvent_t animEvent, Activity mainActivity ) { return ACT_DEPLOY_GREN; }

	virtual bool CanUseOnSelf( CCSPlayer *pPlayer )	{ return true; }
	virtual void OnStartUse( CCSPlayer *pPlayer ) {}

	virtual float GetUseTimerDuration( void );

#ifndef CLIENT_DLL
	virtual void CompleteUse( CCSPlayer *pPlayer ) {}
#endif

private:
	CWeaponBaseItem( const CWeaponBaseItem & ) {}

	CNetworkVarEmbedded( CountdownTimer, m_UseTimer );
	CNetworkVar( bool, m_bRedraw );	// Draw the weapon again after throwing a grenade

};

inline bool CWeaponBaseItem::HasPrimaryAmmo( void )
{
	return true;
}

inline bool CWeaponBaseItem::CanBeSelected( void )
{
	return true;
}


#endif // _WEAPON_BASE_ITEM_H_
