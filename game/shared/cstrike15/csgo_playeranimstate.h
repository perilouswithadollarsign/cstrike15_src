//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_PLAYERANIMSTATE_H
#define CSGO_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "iplayeranimstate.h"
#include "studio.h"
#include "sequence_Transitioner.h"
#include "cs_weapon_parse.h"

#ifdef CLIENT_DLL
	#define CCSPlayer C_CSPlayer
#endif
class CCSPlayer;

enum animstate_layer_t
{
	ANIMATION_LAYER_AIMMATRIX = 0,
	ANIMATION_LAYER_WEAPON_ACTION,
	ANIMATION_LAYER_WEAPON_ACTION_RECROUCH,
	ANIMATION_LAYER_ADJUST,
	ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL,
	ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB,
	ANIMATION_LAYER_MOVEMENT_MOVE,
	ANIMATION_LAYER_MOVEMENT_STRAFECHANGE,
	ANIMATION_LAYER_WHOLE_BODY,
	ANIMATION_LAYER_FLASHED,
	ANIMATION_LAYER_FLINCH,
	ANIMATION_LAYER_ALIVELOOP,
	ANIMATION_LAYER_LEAN,
	ANIMATION_LAYER_COUNT,
};

#define USE_ANIMLAYER_RAW_INDEX false
typedef const int* animlayerpreset;
#define get_animlayerpreset( _n ) s_animLayerOrder ## _n
#define REGISTER_ANIMLAYER_ORDER( _n ) static const int s_animLayerOrder ## _n [ANIMATION_LAYER_COUNT]

REGISTER_ANIMLAYER_ORDER( Default ) = {	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };

// animations can trigger the player to re-order their layers according to hardcoded presets, e.g.:
REGISTER_ANIMLAYER_ORDER( WeaponPost ) = {
	ANIMATION_LAYER_AIMMATRIX,
	ANIMATION_LAYER_WEAPON_ACTION_RECROUCH,
	ANIMATION_LAYER_ADJUST,
	ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL,
	ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB,
	ANIMATION_LAYER_MOVEMENT_MOVE,
	ANIMATION_LAYER_MOVEMENT_STRAFECHANGE,
	ANIMATION_LAYER_WEAPON_ACTION,
	ANIMATION_LAYER_WHOLE_BODY,
	ANIMATION_LAYER_FLASHED,
	ANIMATION_LAYER_FLINCH,
	ANIMATION_LAYER_ALIVELOOP,
	ANIMATION_LAYER_LEAN,
};

enum animstate_pose_param_idx_t
{
	PLAYER_POSE_PARAM_FIRST = 0,
	PLAYER_POSE_PARAM_LEAN_YAW = PLAYER_POSE_PARAM_FIRST,
	PLAYER_POSE_PARAM_SPEED,
	PLAYER_POSE_PARAM_LADDER_SPEED,
	PLAYER_POSE_PARAM_LADDER_YAW,
	PLAYER_POSE_PARAM_MOVE_YAW,
	PLAYER_POSE_PARAM_RUN,
	PLAYER_POSE_PARAM_BODY_YAW,
	PLAYER_POSE_PARAM_BODY_PITCH,
	PLAYER_POSE_PARAM_DEATH_YAW,
	PLAYER_POSE_PARAM_STAND,
	PLAYER_POSE_PARAM_JUMP_FALL,
	PLAYER_POSE_PARAM_AIM_BLEND_STAND_IDLE,
	PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_IDLE,
	PLAYER_POSE_PARAM_STRAFE_DIR,
	PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK,
	PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN,
	PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK,
	PLAYER_POSE_PARAM_MOVE_BLEND_WALK,
	PLAYER_POSE_PARAM_MOVE_BLEND_RUN,
	PLAYER_POSE_PARAM_MOVE_BLEND_CROUCH_WALK,
	//PLAYER_POSE_PARAM_STRAFE_CROSS,
	PLAYER_POSE_PARAM_COUNT,
};

struct animstate_pose_param_cache_t
{
	bool		m_bInitialized;
	int			m_nIndex;
	const char* m_szName;

	animstate_pose_param_cache_t()
	{
		m_bInitialized = false;
		m_nIndex = -1;
		m_szName = "";
	}

	int		GetIndex( void );
	float	GetValue( CCSPlayer* pPlayer );
	void	SetValue( CCSPlayer* pPlayer, float flValue );
	bool	Init( CCSPlayer* pPlayer, const char* szPoseParamName );
};

#define MAX_ANIMSTATE_ANIMNAME_CHARS 64

#define ANIM_TRANSITION_WALK_TO_RUN 0
#define ANIM_TRANSITION_RUN_TO_WALK 1

//these correlate to the CSWeaponType enum
const char* const g_szWeaponPrefixLookupTable[] = {
	"knife",
	"pistol",
	"smg",
	"rifle",
	"shotgun",
	"sniper",
	"heavy",
	"c4",
	"grenade",
	"knife"
};

struct procedural_foot_t
{
	Vector m_vecPosAnim;
	Vector m_vecPosAnimLast;
	Vector m_vecPosPlant;
	Vector m_vecPlantVel;
	float m_flLockAmount;
	float m_flLastPlantTime;

	procedural_foot_t()
	{
		m_vecPosAnim.Init();
		m_vecPosAnimLast.Init();
		m_vecPosPlant.Init();
		m_vecPlantVel.Init();
		m_flLockAmount = 0;
		m_flLastPlantTime = gpGlobals->curtime;
	}

	void Init( Vector vecNew )
	{
		m_vecPosAnim = vecNew;
		m_vecPosAnimLast = vecNew;
		m_vecPosPlant = vecNew;
		m_vecPlantVel.Init();
		m_flLockAmount = 0;
		m_flLastPlantTime = gpGlobals->curtime;
	}
};

struct aimmatrix_transition_t
{
	float	m_flDurationStateHasBeenValid;
	float	m_flDurationStateHasBeenInValid;
	float	m_flHowLongToWaitUntilTransitionCanBlendIn;
	float	m_flHowLongToWaitUntilTransitionCanBlendOut;
	float	m_flBlendValue;

	void UpdateTransitionState( bool bStateShouldBeValid, float flTimeInterval, float flSpeed )
	{
		if ( bStateShouldBeValid )
		{
			m_flDurationStateHasBeenInValid = 0;
			m_flDurationStateHasBeenValid += flTimeInterval;
			if ( m_flDurationStateHasBeenValid >= m_flHowLongToWaitUntilTransitionCanBlendIn )
			{
				m_flBlendValue = Approach( 1, m_flBlendValue, flSpeed );
			}
		}
		else
		{
			m_flDurationStateHasBeenValid = 0;
			m_flDurationStateHasBeenInValid += flTimeInterval;
			if ( m_flDurationStateHasBeenInValid >= m_flHowLongToWaitUntilTransitionCanBlendOut )
			{
				m_flBlendValue = Approach( 0, m_flBlendValue, flSpeed );
			}
		}
	}

	void Init( void )
	{
		m_flDurationStateHasBeenValid = 0;
		m_flDurationStateHasBeenInValid = 0;
		m_flHowLongToWaitUntilTransitionCanBlendIn = 0.3f;
		m_flHowLongToWaitUntilTransitionCanBlendOut = 0.3f;
		m_flBlendValue = 0;
	}

	aimmatrix_transition_t()
	{
		Init();
	}
};

class CCSGOPlayerAnimState
{

public:

	DECLARE_CLASS_NOBASE( CCSGOPlayerAnimState );
	CCSGOPlayerAnimState( CCSPlayer *pPlayer );

	void					Reset( void );
	void					Release( void ) { delete this; }
	void					Update( float eyeYaw, float eyePitch, bool bForce = false );

	float					GetPrimaryCycle( void ) { return m_flPrimaryCycle; }

	void					SetUpVelocity( void );
	void					SetUpAimMatrix( void );
	void					SetUpWeaponAction( void );
	void					SetUpMovement( void );
	void					SetUpAliveloop( void );
	void					SetUpWholeBodyAction( void );
	void					SetUpFlashedReaction( void );
	void					SetUpFlinch( void );
	void					SetUpLean( void );

	void					UpdateAnimLayer( animstate_layer_t nLayerIndex, int nSequence, float flPlaybackRate, float flWeight, float flCycle = 0 );
	void					IncrementLayerCycleWeightRateGeneric( animstate_layer_t nLayerIndex );
	void					IncrementLayerCycle( animstate_layer_t nLayerIndex, bool bAllowLoop = false );
	void					IncrementLayerWeight( animstate_layer_t nLayerIndex );
	void					SetLayerRate( animstate_layer_t nLayerIndex, float flRate );
	void					SetLayerCycle( animstate_layer_t nLayerIndex, float flCycle );
	void					SetLayerWeight( animstate_layer_t nLayerIndex, float flWeight );
	void					SetLayerWeightRate( animstate_layer_t nLayerIndex, float flPrevious );
	void					SetLayerSequence( animstate_layer_t nLayerIndex, int nSequence );
	float					GetLayerWeight( animstate_layer_t nLayerIndex );
	float					GetLayerIdealWeightFromSeqCycle( animstate_layer_t nLayerIndex );
	float					GetLayerRate( animstate_layer_t nLayerIndex );
	float					GetLayerCycle( animstate_layer_t nLayerIndex );
	int						GetLayerSequence( animstate_layer_t nLayerIndex );
	Activity				GetLayerActivity( animstate_layer_t nLayerIndex );
	bool					IsLayerSequenceCompleted( animstate_layer_t nLayerIndex );
	bool					LayerSequenceHasActMod( animstate_layer_t nLayerIndex, const char* szActMod );

	void					WriteAnimstateDebugBuffer( char *pDest );

	void					ApplyLayerOrderPreset( animlayerpreset nNewPreset, bool bForce = false );
	void					UpdateLayerOrderPreset( animstate_layer_t nLayerIndex, int nSequence );
	animlayerpreset			m_pLayerOrderPreset;

	bool					m_bFirstRunSinceInit;

	void					ModifyEyePosition( Vector& vecInputEyePos );

#ifdef CLIENT_DLL
	bool					m_bFirstFootPlantSinceInit;
	void					DoProceduralFootPlant( matrix3x4a_t boneToWorld[], mstudioikchain_t *pLeftFootChain, mstudioikchain_t *pRightFootChain, BoneVector pos[] );
	int						m_iLastUpdateFrame;
	
	float					m_flEyePositionSmoothLerp;
	void					OnClientWeaponChange( CWeaponCSBase* pCurrentWeapon );

	float					m_flStrafeChangeWeightSmoothFalloff;

	void					NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence );
	void					NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight );
	void					NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewcycle );

#endif

#ifndef CLIENT_DLL
	void					AddActivityModifier( const char *szName );
	void					UpdateActivityModifiers( void );
	int						SelectSequenceFromActMods( Activity iAct );
	void					DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );
	void					SelectDeathPose( const CTakeDamageInfo &info, int hitgroup, Activity& activity, float& yaw );
#endif

#ifndef CLIENT_DLL
	float					m_flFlashedAmountEaseOutStart;
	float					m_flFlashedAmountEaseOutEnd;
#endif

private:

	aimmatrix_transition_t	m_tStandWalkAim;
	aimmatrix_transition_t	m_tStandRunAim;
	aimmatrix_transition_t	m_tCrouchWalkAim;

	bool					LayerToIndex( const CAnimationLayer* pLayer, int &nIndex );

	float					LerpCrouchWalkRun( float flCrouchVal, float flWalkVal, float flRunVal );

	bool					CacheSequences( void );
	const char*				GetWeaponPrefix( void );
	int						m_cachedModelIndex;

#ifdef CLIENT_DLL
	float					FootBarrierEq( float flIn, float flMinWidth );
	float					m_flStepHeightLeft;
	float					m_flStepHeightRight;

	CWeaponCSBase*			m_pWeaponLastBoneSetup;
#endif

	CCSPlayer*				m_pPlayer;
	CWeaponCSBase*			m_pWeapon;
	CWeaponCSBase*			m_pWeaponLast;

	float					m_flLastUpdateTime;
	int						m_nLastUpdateFrame;
	float					m_flLastUpdateIncrement;

	float					m_flEyeYaw;
	float					m_flEyePitch;
	float					m_flFootYaw;
	float					m_flFootYawLast;
	float					m_flMoveYaw;
	float					m_flMoveYawIdeal;
	float					m_flMoveYawCurrentToIdeal;
	float					m_flTimeToAlignLowerBody;

	float					m_flPrimaryCycle;
	float					m_flMoveWeight;
	float					m_flMoveWeightSmoothed;
	float					m_flAnimDuckAmount;
	float					m_flDuckAdditional;
	float					m_flRecrouchWeight;

	Vector					m_vecPositionCurrent;
	Vector					m_vecPositionLast;

	Vector					m_vecVelocity;
	Vector					m_vecVelocityNormalized;
	Vector					m_vecVelocityNormalizedNonZero;
	float					m_flVelocityLengthXY;
	float					m_flVelocityLengthZ;

	float					m_flSpeedAsPortionOfRunTopSpeed;
	float					m_flSpeedAsPortionOfWalkTopSpeed;
	float					m_flSpeedAsPortionOfCrouchTopSpeed;

	float					m_flDurationMoving;
	float					m_flDurationStill;

	bool					m_bOnGround;
#ifndef CLIENT_DLL
	bool					m_bJumping;
	float					m_flLowerBodyRealignTimer;
#endif
	bool					m_bLanding;
	float					m_flJumpToFall;
	float					m_flDurationInAir;
	float					m_flLeftGroundHeight;
	float					m_flLandAnimMultiplier;

	float					m_flWalkToRunTransition;

	bool					m_bLandedOnGroundThisFrame;
	bool					m_bLeftTheGroundThisFrame;
	float					m_flInAirSmoothValue;

	bool					m_bOnLadder;
	float					m_flLadderWeight;
	float					m_flLadderSpeed;

	bool					m_bWalkToRunTransitionState;

	bool					m_bDefuseStarted;
	bool					m_bPlantAnimStarted;
	bool					m_bTwitchAnimStarted;
	bool					m_bAdjustStarted;

	CUtlVector<CUtlSymbol>	m_ActivityModifiers;

	float					m_flNextTwitchTime;

	float					m_flTimeOfLastKnownInjury;

	float					m_flLastVelocityTestTime;
	Vector					m_vecVelocityLast;
	Vector					m_vecTargetAcceleration;
	Vector					m_vecAcceleration;
	float					m_flAccelerationWeight;

	float					m_flAimMatrixTransition;
	float					m_flAimMatrixTransitionDelay;

	bool					m_bFlashed;

	float					m_flStrafeChangeWeight;
	float					m_flStrafeChangeTargetWeight;
	float					m_flStrafeChangeCycle;
	int						m_nStrafeSequence;
	bool					m_bStrafeChanging;
	float					m_flDurationStrafing;

	float					m_flFootLerp;

	bool					m_bFeetCrossed;

	bool					m_bPlayerIsAccelerating;

	animstate_pose_param_cache_t m_tPoseParamMappings[ PLAYER_POSE_PARAM_COUNT ];

#ifndef CLIENT_DLL
	bool					m_bDeployRateLimiting;
#endif

	float					m_flDurationMoveWeightIsTooHigh;
	float					m_flStaticApproachSpeed;

	int						m_nPreviousMoveState;
	float					m_flStutterStep;

	float					m_flActionWeightBiasRemainder;

#ifdef CLIENT_DLL
	procedural_foot_t	m_footLeft;
	procedural_foot_t	m_footRight;

	float					m_flCameraSmoothHeight;
	bool					m_bSmoothHeightValid;
	float					m_flLastTimeVelocityOverTen;
#endif

	float					m_flAimYawMin;
	float					m_flAimYawMax;
	float					m_flAimPitchMin;
	float					m_flAimPitchMax;

	//float					m_flMoveWalkWeight;
	//float					m_flMoveCrouchWalkWeight;
	//float					m_flMoveRunWeight;

	int						m_nAnimstateModelVersion;
};

CCSGOPlayerAnimState *CreateCSGOPlayerAnimstate( CBaseAnimatingOverlay *pEntity );

#endif // CSGO_PLAYERANIMSTATE_H

