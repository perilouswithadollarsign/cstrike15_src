//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPONS_PORTAL_H
#define WEAPONS_PORTAL_H

#ifdef _WIN32
#pragma once
#endif
typedef enum
{
	WEAPON_NONE = 0,

	//Melee
	WEAPON_CROWBAR,

	//Special
	WEAPON_PORTALGUN,
	WEAPON_PHYSCANNON,

	//Pistols
	WEAPON_PISTOL,
	WEAPON_357,	

	//Machineguns
	WEAPON_SMG,
	WEAPON_AR2,

	//Grenades
	WEAPON_FRAG,
	WEAPON_BUGBAIT,

	//Other
	WEAPON_SHOTGUN,
	WEAPON_CROSSBOW,
	WEAPON_RPG,

	//Hat
	WEAPON_WEARABLE,

	WEAPON_MAX,		// number of weapons weapon index

} PortalWeaponID;

#endif // WEAPONS_PORTAL_H