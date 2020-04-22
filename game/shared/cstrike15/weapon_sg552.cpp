//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )

	#define CWeaponSG552 C_WeaponSG552
	#include "c_cs_player.h"

#else

	#include "cs_player.h"

#endif


class CWeaponSG552 : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CWeaponSG552, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponSG552();

	virtual void PrimaryAttack();
	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_SG552; }

#ifdef CLIENT_DLL
	virtual bool	DoesHideViewModelWhenZoomed( void ) { return false; }
#endif

private:

	CWeaponSG552( const CWeaponSG552 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSG552, DT_WeaponSG552 )

BEGIN_NETWORK_TABLE( CWeaponSG552, DT_WeaponSG552 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSG552 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_sg552, WeaponSG552 );
// PRECACHE_REGISTER( weapon_sg552 );



CWeaponSG552::CWeaponSG552()
{
}


void CWeaponSG552::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	bool bZoomed = pPlayer->GetFOV() < pPlayer->GetDefaultFOV();

	float flCycleTime = GetCycleTime();

	if ( bZoomed )
		flCycleTime = 0.135f;

	CSBaseGunFire( flCycleTime, m_weaponMode );
}
