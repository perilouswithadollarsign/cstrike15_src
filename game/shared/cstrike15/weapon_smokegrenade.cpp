//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "weapon_smokegrenade.h"


#ifdef CLIENT_DLL
	
#else

	#include "cs_player.h"
	#include "items.h"
	#include "smokegrenade_projectile.h"

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( SmokeGrenade, DT_SmokeGrenade )

BEGIN_NETWORK_TABLE(CSmokeGrenade, DT_SmokeGrenade)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CSmokeGrenade )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_smokegrenade, SmokeGrenade );
PRECACHE_REGISTER( weapon_smokegrenade );


#ifndef CLIENT_DLL

	BEGIN_DATADESC( CSmokeGrenade )
	END_DATADESC()

	void CSmokeGrenade::EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo )
	{
		CSmokeGrenadeProjectile::Create( vecSrc, vecAngles, vecVel, angImpulse, pPlayer, weaponInfo );
	}

#endif

