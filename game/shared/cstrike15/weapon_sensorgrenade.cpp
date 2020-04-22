//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "weapon_csbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "weapon_sensorgrenade.h"

#ifdef CLIENT_DLL
	
#else
	#include "cs_player.h"
	#include "items.h"
	#include "sensorgrenade_projectile.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( SensorGrenade, DT_SensorGrenade )

BEGIN_NETWORK_TABLE(CSensorGrenade, DT_SensorGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CSensorGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_tagrenade, SensorGrenade );
PRECACHE_REGISTER( weapon_tagrenade );

#if !defined( CLIENT_DLL )

BEGIN_DATADESC( CSensorGrenade )
END_DATADESC()

void CSensorGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	CSensorGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

void CSensorGrenade::ShotDetonate( CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	CSensorGrenadeProjectile* pGrenade = CSensorGrenadeProjectile::Create( this->GetAbsOrigin(), this->GetAbsAngles(), Vector(0,0,0), AngularImpulse(0,0,0), pPlayer, weaponInfo );
	if ( pGrenade )
		pGrenade->DetonateOnNextThink();
}

#endif // !CLIENT_DLL