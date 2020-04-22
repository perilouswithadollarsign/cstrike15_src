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
#include "weapon_decoy.h"

#ifdef CLIENT_DLL
	
#else
	#include "cs_player.h"
	#include "items.h"
	#include "decoy_projectile.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DecoyGrenade, DT_DecoyGrenade )

BEGIN_NETWORK_TABLE(CDecoyGrenade, DT_DecoyGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CDecoyGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_decoy, DecoyGrenade );
PRECACHE_REGISTER( weapon_decoy );

#if !defined( CLIENT_DLL )

BEGIN_DATADESC( CDecoyGrenade )
END_DATADESC()

void CDecoyGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
{
	CDecoyProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
}

#endif // !CLIENT_DLL