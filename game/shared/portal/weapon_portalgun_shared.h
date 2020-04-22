//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_PORTALGUN_SHARED_H
#define WEAPON_PORTALGUN_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "portal_shareddefs.h"

#if defined( CLIENT_DLL )
class C_InfoPlacementHelper;
#define CInfoPlacementHelper C_InfoPlacementHelper
#else
class CInfoPlacementHelper;
#endif

struct TracePortalPlacementInfo_t
{
	// Default initialization
	TracePortalPlacementInfo_t( void ) :
		ePlacementResult( PORTAL_PLACEMENT_SUCCESS ),
		vecFinalPosition( vec3_invalid ),
		angFinalAngles( vec3_angle ),
		pPlacementHelper( NULL )
	{ 
		UTIL_ClearTrace( tr );
	}

	PortalPlacementResult_t	ePlacementResult;	// The final indicator of if the portal succeeded in placement
	Vector					vecFinalPosition;	// Where the shot ended up
	QAngle					angFinalAngles;		// How the shot ended up oriented
	CInfoPlacementHelper	*pPlacementHelper;	// A placement helper (if we hit it)
	trace_t					tr;					// Resultant trace
};

#ifdef CLIENT_DLL
#include "c_weapon_portalgun.h"
#else
#include "weapon_portalgun.h"
#endif

//
// NOTE: IF you change these, you *MUST* ensure the Precache blocks in CWeaponPortalgun::Precache()
// are maintained. Lack of precaching causes VERY BAD run time I/O hitches which cause the audio
// to stutter and pollute Perf analysis.
//
#define PORTALGUN_BEAM_SPRITE "sprites/grav_beam.vmt"
#define PORTALGUN_BEAM_SPRITE_NOZ "sprites/grav_beam_noz.vmt"
#define PORTALGUN_GLOW_SPRITE "sprites/glow04_noz"
#define PORTALGUN_ENDCAP_SPRITE "sprites/grav_flare"
#define PORTALGUN_GRAV_ACTIVE_GLOW "sprites/grav_light"
#define PORTALGUN_PORTAL1_FIRED_LAST_GLOW "sprites/bluelight"
#define PORTALGUN_PORTAL2_FIRED_LAST_GLOW "sprites/orangelight"
#define PORTALGUN_PORTAL_TINTED_GLOW "sprites/whitelight"
// #define PORTALGUN_PORTAL_MUZZLE_GLOW_SPRITE "sprites/portalgun_effects"
#define PORTALGUN_PORTAL_MUZZLE_GLOW_SPRITE "particle/particle_glow_05"
#define PORTALGUN_PORTAL_TUBE_BEAM_SPRITE "particle/particle_glow_05"

enum
{
	EFFECT_NONE,
	EFFECT_READY,
	EFFECT_HOLDING,
};

extern ConVar sk_auto_reload_time;

#endif // WEAPON_PORTALGUN_SHARED_H
