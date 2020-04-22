//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IPLAYERANIMSTATE_H
#define IPLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


typedef enum
{
	LEGANIM_9WAY,		// Legs use a 9-way blend, with "move_x" and "move_y" pose parameters.
	LEGANIM_8WAY,		// Legs use an 8-way blend with "move_yaw" pose param.
	LEGANIM_GOLDSRC	// Legs always point in the direction he's running and the torso rotates.
} LegAnimType_t;


#ifdef CSTRIKE15
enum PlayerAnimEvent_t
{
	PLAYERANIMEVENT_FIRE_GUN_PRIMARY=0,
	PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT, // an optional primary attack for variation in animation. For example, the knife toggles between left AND right slash animations.
	PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1,
	PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1, // an optional primary special attack for variation in animation.
	PLAYERANIMEVENT_FIRE_GUN_SECONDARY,
	PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1,
	PLAYERANIMEVENT_GRENADE_PULL_PIN,
	PLAYERANIMEVENT_THROW_GRENADE,
	PLAYERANIMEVENT_JUMP,
	PLAYERANIMEVENT_RELOAD,
	PLAYERANIMEVENT_RELOAD_START,	///< w_model partial reload for shotguns
	PLAYERANIMEVENT_RELOAD_LOOP,	///< w_model partial reload for shotguns
	PLAYERANIMEVENT_RELOAD_END,		///< w_model partial reload for shotguns
	PLAYERANIMEVENT_CLEAR_FIRING,	///< clear animations on the firing layer
	PLAYERANIMEVENT_DEPLOY,			///< Used to play deploy animations on third person models.
	PLAYERANIMEVENT_SILENCER_ATTACH,
	PLAYERANIMEVENT_SILENCER_DETACH,
	
	// new events
	PLAYERANIMEVENT_THROW_GRENADE_UNDERHAND,
	PLAYERANIMEVENT_CATCH_WEAPON,
	PLAYERANIMEVENT_COUNT
};
#endif


abstract_class IPlayerAnimState
{
public:
	virtual void Release() = 0;

	// Update() and DoAnimationEvent() together maintain the entire player's animation state.
	//
	// Update() maintains the the lower body animation (the player's m_nSequence)
	// and the upper body overlay based on the player's velocity and look direction.
	//
	// It also modulates these based on events triggered by DoAnimationEvent.
	virtual void Update( float eyeYaw, float eyePitch ) = 0;

	virtual void ModifyTauntDuration( float flTimingChange ) {}

	// This is called by the client when a new player enters the PVS to clear any events
	// the dormant version of the entity may have been playing.
	virtual void ClearAnimationState() = 0;

	// The client uses this to figure out what angles to render the entity with (since as the guy turns,
	// it will change his body_yaw pose parameter before changing his rendered angle).
	virtual const QAngle& GetRenderAngles() = 0;

	virtual void SetForceAimYaw( bool bForce ) = 0;

#ifdef CSTRIKE15
	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 ) = 0;

	// Returns true if we're playing the grenade prime or throw animation.
	virtual bool IsThrowingGrenade() = 0;
	virtual bool ShouldHideGrenadeDuringThrow() = 0;
#endif
};


#endif // IPLAYERANIMSTATE_H
