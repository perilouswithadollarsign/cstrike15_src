//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef WEAPON_SENSORGRENADE_H
#define WEAPON_SENSORGRENADE_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_basecsgrenade.h"

#ifdef CLIENT_DLL
	#define CSensorGrenade C_SensorGrenade
#endif

//-----------------------------------------------------------------------------
// SensorGrenade grenades
//-----------------------------------------------------------------------------
class CSensorGrenade : public CBaseCSGrenade
{
public:
	DECLARE_CLASS( CSensorGrenade, CBaseCSGrenade );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CSensorGrenade() {}

	virtual CSWeaponID GetCSWeaponID( void ) const { return WEAPON_TAGRENADE; }

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();

	virtual void EmitGrenade( Vector vecSrc, QAngle vecAngles, Vector vecVel, AngularImpulse angImpulse, CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
	virtual void ShotDetonate( CBasePlayer *pPlayer, const CCSWeaponInfo& weaponInfo );
#endif

private:
	CSensorGrenade( const CSensorGrenade& );
};

#endif // WEAPON_SENSORGRENADE_H
