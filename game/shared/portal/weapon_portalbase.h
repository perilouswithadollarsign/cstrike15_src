//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_PORTALBASE_H
#define WEAPON_PORTALBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "basecombatweapon_shared.h"
#include "portal_weapon_parse.h"

#if defined( CLIENT_DLL )
	#define CWeaponPortalBase C_WeaponPortalBase
	void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip );
#endif

class CPortal_Player;

// These are the names of the ammo types that go in the CAmmoDefs and that the 
// weapon script files reference.

// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// MIKETODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );

#include "weapons_portal.h"

class CWeaponPortalBase : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponPortalBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponPortalBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();
	
		void SendReloadSoundEvent( void );

		void Materialize( void );
	#endif

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const;

	CBasePlayer* GetPlayerOwner() const;
	CPortal_Player* GetPortalPlayerOwner() const;

	// Get specific Portal weapon ID (ie: WEAPON_PORTALGUN, etc)
	virtual int GetWeaponID( void ) const { return WEAPON_NONE; }

	void WeaponSound( WeaponSound_t sound_type, float soundtime = 0.0f );
	
	CPortalSWeaponInfo const	&GetPortalWpnData() const;

	virtual void FireBullets( const FireBulletsInfo_t &info );
	
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_FORCE_TRANSITION; }

public:
	#if defined( CLIENT_DLL )
	
		virtual IClientModelRenderable*	GetClientModelRenderable();
		virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
		virtual bool	ShouldDrawCrosshair( void ) { return true; }
		virtual bool	ShouldPredict();
		virtual void	OnDataChanged( DataUpdateType_t type );
		virtual void	DrawCrosshair();

		virtual void	DoAnimationEvents( CStudioHdr *pStudio );
		virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );

		virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );

	#else

		virtual void	Spawn();

	#endif

	float		m_flPrevAnimTime;
	float  m_flNextResetCheckTime;

	Vector	GetOriginalSpawnOrigin( void ) { return m_vOriginalSpawnOrigin;	}
	QAngle	GetOriginalSpawnAngles( void ) { return m_vOriginalSpawnAngles;	}

private:

	CWeaponPortalBase( const CWeaponPortalBase & );

	Vector m_vOriginalSpawnOrigin;
	QAngle m_vOriginalSpawnAngles;
};


#endif // WEAPON_PORTALBASE_H
