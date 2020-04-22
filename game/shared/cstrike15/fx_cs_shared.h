//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FX_CS_SHARED_H
#define FX_CS_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#ifdef CLIENT_DLL
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
#endif


// This runs on both the client and the server.
// On the server, it only does the damage calculations.
// On the client, it does all the effects.
void FX_FireBullets( 
	int	iPlayer,
	uint16 nItemDefIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	CSWeaponID iWeaponID,
	int	iMode,
	int iSeed,
	float fInaccuracy,
	float fSpread,
	float fAccuracyFishtail,
	float flSoundTime/* = 0.0f*/,
	WeaponSound_t sound_type/* = SINGLE*/,
	float flRecoilIndex/* = 0.0f*/
	);

// This runs on both the client and the server.
// On the server, it dispatches a TE_PlantBomb to visible clients.
// On the client, it plays the planting animation.
enum PlantBombOption_t
{
	PLANTBOMB_PLANT, // play the planting animation
	PLANTBOMB_ABORT, // abort the planting animation
	// NOTE: If you add additional items to this enum then m_option in CTEPlantBomb will need to have its SendPropInt setting changed to have more than one bit.
};
void FX_PlantBomb( int iPlayer, const Vector &vOrigin, PlantBombOption_t option );

#endif // FX_CS_SHARED_H
