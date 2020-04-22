//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPLAYER_SHARED_H
#define BASEPLAYER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// PlayerUse defines
#ifdef PORTAL2
#define	PLAYER_USE_RADIUS	100.f
#else
#define	PLAYER_USE_RADIUS	80.f
#define	PLAYER_USE_BOT_RADIUS	140.f
#endif // PORTAL2

#define CONE_45_DEGREES		0.707f
#define CONE_15_DEGREES		0.9659258f
#define CONE_90_DEGREES		0

#define TRAIN_ACTIVE	0x80 
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL	0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM	0x03
#define TRAIN_FAST		0x04 
#define TRAIN_BACK		0x05

// entity messages
#define UPDATE_PLAYER_RADAR	2

#define DEATH_ANIMATION_TIME	3.0f

// multiplayer only
#define NOINTERP_PARITY_MAX			4
#define NOINTERP_PARITY_MAX_BITS	2

struct autoaim_params_t
{
	autoaim_params_t()
	{
		m_fScale = 0;
		m_fMaxDist = 0;
		m_fMaxDeflection = -1.0f;
		m_bOnTargetQueryOnly = false;
	}

	Vector		m_vecAutoAimDir;		// Output: The direction autoaim wishes to point.
	Vector		m_vecAutoAimPoint;		// Output: The point (world space) that autoaim is aiming at.
	EHANDLE		m_hAutoAimEntity;		// Output: The entity that autoaim is aiming at.
	float		m_fScale;				// Input:
	float		m_fMaxDist;				// Input:
	float		m_fMaxDeflection;		// Input:
	bool		m_bOnTargetQueryOnly;	// Input: Don't do expensive assistance, just resolve m_bOnTargetNatural
	bool		m_bAutoAimAssisting;	// Output: If this is true, autoaim is aiming at the target.
	bool		m_bOnTargetNatural;		// Output: If true, the player is on target without assistance.
};

enum stepsoundtimes_t
{
	STEPSOUNDTIME_NORMAL = 0,
	STEPSOUNDTIME_ON_LADDER,
	STEPSOUNDTIME_WATER_KNEE,
	STEPSOUNDTIME_WATER_FOOT,
};

//
// Player PHYSICS FLAGS bits
//
enum PlayerPhysFlag_e
{
	PFLAG_DIROVERRIDE	= ( 1<<0 ),		// override the player's directional control (trains, physics gun, etc.)
	PFLAG_DUCKING		= ( 1<<1 ),		// In the process of ducking, but totally squatted yet
	PFLAG_USING			= ( 1<<2 ),		// Using a continuous entity
	PFLAG_OBSERVER		= ( 1<<3 ),		// player is locked in stationary cam mode. Spectators can move, observers can't.
	PFLAG_VPHYSICS_MOTIONCONTROLLER = ( 1<<4 ),	// player is physically attached to a motion controller
	PFLAG_GAMEPHYSICS_ROTPUSH = (1<<5), // game physics did a rotating push that we may want to override with vphysics

	// If you add another flag here check that you aren't 
	// overwriting phys flags in the HL2 of TF2 player classes
};

enum
{
	VPHYS_WALK = 0,
	VPHYS_CROUCH,
	VPHYS_NOCLIP,
};

// useful cosines
#define DOT_1DEGREE   0.9998476951564
#define DOT_2DEGREE   0.9993908270191
#define DOT_3DEGREE   0.9986295347546
#define DOT_4DEGREE   0.9975640502598
#define DOT_5DEGREE   0.9961946980917
#define DOT_6DEGREE   0.9945218953683
#define DOT_7DEGREE   0.9925461516413
#define DOT_8DEGREE   0.9902680687416
#define DOT_9DEGREE   0.9876883405951
#define DOT_10DEGREE  0.9848077530122
#define DOT_15DEGREE  0.9659258262891
#define DOT_20DEGREE  0.9396926207859
#define DOT_25DEGREE  0.9063077870367
#define DOT_30DEGREE  0.866025403784
#define DOT_45DEGREE  0.707106781187

//#define DEBUG_MOTION_CONTROLLERS  //uncomment to spew debug data while in a motion controller


enum HltvUiType_t
{
	HLTV_UI_XRAY_ON = 0,
	HLTV_UI_XRAY_OFF,
	HLTV_UI_SCOREBOARD_ON,
	HLTV_UI_SCOREBOARD_OFF,
	HLTV_UI_OVERVIEW_ON,
	HLTV_UI_OVERVIEW_OFF,
	HLTV_UI_GRAPHS_ON,
	HLTV_UI_GRAPHS_OFF
};


// Shared header file for players
#if defined( CLIENT_DLL )
#define CBasePlayer C_BasePlayer
#include "c_baseplayer.h"
#else
#include "player.h"
#endif

#endif // BASEPLAYER_SHARED_H
