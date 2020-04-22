//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Declares the base class for prop paint power users that the player can pick up.
//
//=============================================================================//
#ifndef PLAYER_PICKUP_PAINT_POWER_USER_H
#define PLAYER_PICKUP_PAINT_POWER_USER_H

#ifndef CLIENT_DLL
#include "player_pickup.h"
#endif

#include "prop_paint_power_user.h"

template< typename BasePlayerPickupType >
class PlayerPickupPaintPowerUser : public PropPaintPowerUser< BasePlayerPickupType >
{
	DECLARE_CLASS( PlayerPickupPaintPowerUser< BasePlayerPickupType >, PropPaintPowerUser< BasePlayerPickupType > );

public:
	//-------------------------------------------------------------------------
	// IPlayerPickupVphysics Overrides
	//-------------------------------------------------------------------------
	virtual void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
};

template< typename BasePlayerPickupType >
void PlayerPickupPaintPowerUser<BasePlayerPickupType>::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	BaseClass::OnPhysGunPickup( pPhysGunUser, reason );
}

#endif // ifndef PLAYER_PICKUP_PAINT_POWER_USER_H
