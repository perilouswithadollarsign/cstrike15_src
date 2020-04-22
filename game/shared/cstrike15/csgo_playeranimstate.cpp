 //========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "csgo_playeranimstate.h"
#include "iplayeranimstate.h"
#include "animation.h"
#include "weapon_csbase.h"
#include "gamemovement.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
	#include "ilagcompensationmanager.h"
#endif

extern CUtlSymbolTable g_ActivityModifiersTable;

float ClampCycle( float flCycleIn )
{
	flCycleIn -= int(flCycleIn);

	if ( flCycleIn < 0 )
	{
		flCycleIn += 1;
	}
	else if ( flCycleIn > 1 )
	{
		flCycleIn -= 1;
	}

	return flCycleIn;
}

CCSGOPlayerAnimState *CreateCSGOPlayerAnimstate( CBaseAnimatingOverlay *pEntity )
{
	CCSPlayer *pPlayer = ToCSPlayer( pEntity );
	Assert( pPlayer );

	CCSGOPlayerAnimState *pRet = new CCSGOPlayerAnimState( pPlayer );
	return pRet;
}

#define CURRENT_ANIMSTATE_VERSION 2
CCSGOPlayerAnimState::CCSGOPlayerAnimState( CCSPlayer *pPlayer )
{
	m_pPlayer = pPlayer;
	Assert( m_pPlayer );

	m_cachedModelIndex = -1;
	m_nAnimstateModelVersion = CURRENT_ANIMSTATE_VERSION;
	Reset();
}

#define CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX 58.0f
#define CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN -58.0f
#define CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX 90.0f
#define CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN -90.0f

void CCSGOPlayerAnimState::Reset( void )
{
	Assert( m_pPlayer );

#ifndef CLIENT_DLL
	m_pPlayer->SetNumAnimOverlays( ANIMATION_LAYER_COUNT );
#endif
	ApplyLayerOrderPreset( get_animlayerpreset( Default ), true );

#ifdef CLIENT_DLL
	m_iLastUpdateFrame					= 0;
	m_flStepHeightLeft					= 0;
	m_flStepHeightRight					= 0;
#endif

#ifndef CLIENT_DLL
	m_flFlashedAmountEaseOutStart		= 0;
	m_flFlashedAmountEaseOutEnd			= 0;
#endif

	m_pWeapon							= m_pPlayer->GetActiveCSWeapon();
	m_pWeaponLast = m_pWeapon;

#ifdef CLIENT_DLL
	m_pWeaponLastBoneSetup = m_pWeapon;
	m_flEyePositionSmoothLerp			= 0;
	m_flStrafeChangeWeightSmoothFalloff = 0;
	m_bFirstFootPlantSinceInit			= true;
#endif

	m_flLastUpdateTime					= 0;
	m_flLastUpdateIncrement				= 0;

	m_flEyeYaw							= 0;
	m_flEyePitch						= 0;
	m_flFootYaw							= 0;
	m_flFootYawLast						= 0;
	m_flMoveYaw							= 0;
	m_flMoveYawIdeal					= 0;
	m_flMoveYawCurrentToIdeal			= 0;

#ifndef CLIENT_DLL
	m_pPlayer->m_flLowerBodyYawTarget.Set( 0 );
	m_flLowerBodyRealignTimer			= 0;
#endif

	m_tStandWalkAim.Init();
	m_tStandWalkAim.m_flHowLongToWaitUntilTransitionCanBlendIn = 0.4f;
	m_tStandWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut = 0.2f;
	m_tStandRunAim.Init();
	m_tStandRunAim.m_flHowLongToWaitUntilTransitionCanBlendIn = 0.2f;
	m_tStandRunAim.m_flHowLongToWaitUntilTransitionCanBlendOut = 0.4f;
	m_tCrouchWalkAim.Init();
	m_tCrouchWalkAim.m_flHowLongToWaitUntilTransitionCanBlendIn = 0.3f;
	m_tCrouchWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut = 0.3f;

	m_flPrimaryCycle					= 0;
	m_flMoveWeight						= 0;
	m_flMoveWeightSmoothed				= 0;
	m_flAnimDuckAmount					= 0;
	m_flDuckAdditional					= 0; // for when we duck a bit after landing from a jump
	m_flRecrouchWeight					= 0;

	m_vecPositionCurrent.Init();
	m_vecPositionLast.Init();

	m_vecVelocity.Init();
	m_vecVelocityNormalized.Init();
	m_vecVelocityNormalizedNonZero.Init();
	m_flVelocityLengthXY				= 0;
	m_flVelocityLengthZ					= 0;

	m_flSpeedAsPortionOfRunTopSpeed		= 0;
	m_flSpeedAsPortionOfWalkTopSpeed	= 0;
	m_flSpeedAsPortionOfCrouchTopSpeed	= 0;

	m_flDurationMoving					= 0;
	m_flDurationStill					= 0;

	m_bOnGround							= true;
#ifndef CLIENT_DLL
	m_bJumping							= false;
#endif
	m_flLandAnimMultiplier				= 1.0f;
	m_flLeftGroundHeight				= 0;
	m_bLanding							= false;
	m_flJumpToFall						= 0;
	m_flDurationInAir					= 0;

	m_flWalkToRunTransition				= 0;

	m_bLandedOnGroundThisFrame			= false;
	m_bLeftTheGroundThisFrame			= false;
	m_flInAirSmoothValue				= 0;

	m_bOnLadder							= false;
	m_flLadderWeight					= 0;
	m_flLadderSpeed						= 0;

	m_bWalkToRunTransitionState			= ANIM_TRANSITION_WALK_TO_RUN;

	m_bDefuseStarted					= false;
	m_bPlantAnimStarted					= false;
	m_bTwitchAnimStarted				= false;
	m_bAdjustStarted					= false;

	m_flNextTwitchTime					= 0;

	m_flTimeOfLastKnownInjury			= 0;

	m_flLastVelocityTestTime			= 0;
	m_vecVelocityLast.Init();
	m_vecTargetAcceleration.Init();
	m_vecAcceleration.Init();
	m_flAccelerationWeight				= 0;

	m_flAimMatrixTransition				= 0;
	m_flAimMatrixTransitionDelay		= 0;

	m_bFlashed							= 0;


	m_flStrafeChangeWeight				= 0;
	m_flStrafeChangeTargetWeight		= 0;
	m_flStrafeChangeCycle				= 0;
	m_nStrafeSequence					= -1;
	m_bStrafeChanging					= false;
	m_flDurationStrafing				= 0;

	m_flFootLerp						= 0;

	m_bFeetCrossed						= false;

	m_bPlayerIsAccelerating				= false;

#ifndef CLIENT_DLL
	m_bDeployRateLimiting				= false;
#endif

	m_flDurationMoveWeightIsTooHigh		= 0;
	m_flStaticApproachSpeed				= 80;

	m_flStutterStep						= 0;
	m_nPreviousMoveState				= 0;

	m_flActionWeightBiasRemainder		= 0;

	m_flAimYawMin						= CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN;
	m_flAimYawMax						= CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX;
	m_flAimPitchMin						= CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN;
	m_flAimPitchMax						= CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX;

	//m_flMoveWalkWeight					= 0;
	//m_flMoveCrouchWalkWeight			= 0;
	//m_flMoveRunWeight					= 0;

	m_ActivityModifiers.Purge();

	m_bFirstRunSinceInit				= true;

#ifdef CLIENT_DLL
	m_flCameraSmoothHeight				= 0;
	m_bSmoothHeightValid				= false;
	m_flLastTimeVelocityOverTen			= 0;

	m_pPlayer->ClearAnimLODflags();
#endif
}

#define bonesnapshot_def( _name, _val ) const float _name = _val;
#define bonesnapshot_get( _name ) _name

bonesnapshot_def( cl_bonesnapshot_speed_weaponchange,	0.25 )
bonesnapshot_def( cl_bonesnapshot_speed_strafestart,	0.15 )
bonesnapshot_def( cl_bonesnapshot_speed_movebegin,		0.3 )
bonesnapshot_def( cl_bonesnapshot_speed_ladderenter,	0.25 )
bonesnapshot_def( cl_bonesnapshot_speed_ladderexit,		0.1 )

#define CSGO_ANIM_DEPLOY_RATELIMIT 0.15f

#define CSGO_ANIM_DUCK_APPROACH_SPEED_DOWN 3.1f
#define CSGO_ANIM_DUCK_APPROACH_SPEED_UP 6.0f

void CCSGOPlayerAnimState::Update( float eyeYaw, float eyePitch, bool bForce )
{
	if ( !m_pPlayer )
		return;

	if ( !m_pPlayer->IsAlive() )
		return;

	if ( !CacheSequences() )
		return;

	{
		// Apply recoil angle to aim matrix so bullets still come out of the gun straight while spraying
		eyePitch = AngleNormalize( eyePitch + m_pPlayer->m_flThirdpersonRecoil );
	}


	// don't need to update animstate if we already have this curtime
	if ( !bForce && ( m_flLastUpdateTime == gpGlobals->curtime || m_nLastUpdateFrame == gpGlobals->framecount ) )
		return;
	m_flLastUpdateIncrement = Max( 0.0f, gpGlobals->curtime - m_flLastUpdateTime ); // negative values possible when clocks on client and server go out of sync..

#ifdef CLIENT_DLL
	// suspend bonecache invalidation
	C_BaseAnimating::EnableInvalidateBoneCache( false );
#endif

	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );
	m_vecPositionCurrent = m_pPlayer->GetAbsOrigin();
	m_pWeapon = m_pPlayer->GetActiveCSWeapon();
	

	// purge layer dispatches on weapon change and init
	if ( m_pWeapon != m_pWeaponLast || m_bFirstRunSinceInit )
	{

#ifdef CLIENT_DLL
		// changing weapons will change the pose of leafy bones like fingers. The next time we
		// set up this player's bones, treat it like a clean first setup.
		m_pPlayer->m_nComputedLODframe = 0;
#endif

		for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
		{
			CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( i, USE_ANIMLAYER_RAW_INDEX );
			if ( pLayer )
			{
				pLayer->m_pDispatchedStudioHdr = NULL;
				pLayer->m_nDispatchedSrc = ACT_INVALID;
				pLayer->m_nDispatchedDst = ACT_INVALID;
			}
		}
	}

	
#ifdef CLIENT_DLL
	if ( IsPreCrouchUpdateDemo() )
	{
		// compatibility for old demos using old crouch values
		float flTargetDuck = (m_pPlayer->GetFlags() & ( FL_ANIMDUCKING )) ? 1.0f : m_flDuckAdditional;
		m_flAnimDuckAmount = Approach( flTargetDuck, m_flAnimDuckAmount, m_flLastUpdateIncrement * ( (m_flAnimDuckAmount < flTargetDuck) ? CSGO_ANIM_DUCK_APPROACH_SPEED_DOWN : CSGO_ANIM_DUCK_APPROACH_SPEED_UP )  );
		m_flAnimDuckAmount = clamp( m_flAnimDuckAmount, 0, 1 );
	}
	else
#endif
	{
		m_flAnimDuckAmount = clamp( Approach( clamp( m_pPlayer->m_flDuckAmount + m_flDuckAdditional, 0, 1), m_flAnimDuckAmount, m_flLastUpdateIncrement * 6.0f ), 0, 1 );
	}

	// no matter what, we're always playing 'default' underneath
	{
		MDLCACHE_CRITICAL_SECTION();
		m_pPlayer->SetSequence( 0 );
		m_pPlayer->SetPlaybackRate( 0 );
		m_pPlayer->SetCycle( 0 );
	}

	// all the layers get set up here
	SetUpVelocity();			// calculate speed and set up body yaw values
	SetUpAimMatrix();			// aim matrices are full body, so they not only point the weapon down the eye dir, they can crouch the idle player
	SetUpWeaponAction();		// firing, reloading, silencer-swapping, deploying
	SetUpMovement();			// jumping, climbing, ground locomotion, post-weapon crouch-stand
	SetUpAliveloop();			// breathe and fidget
	SetUpWholeBodyAction();		// defusing, planting, whole-body custom events
	SetUpFlashedReaction();		// shield eyes from flashbang
	SetUpFlinch();				// flinch when shot
	SetUpLean();				// lean into acceleration

#ifdef  CLIENT_DLL
	// zero-sequences are un-set and should have zero weight on the client
	for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
	{
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( i, USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer && pLayer->GetSequence() == 0 )
			pLayer->SetWeight(0);
	}
#endif

	// force abs angles on client and server to line up hitboxes
	m_pPlayer->SetAbsAngles( QAngle( 0, m_flFootYaw, 0 ) );

#ifdef CLIENT_DLL
	// restore bonecache invalidation
	C_BaseAnimating::EnableInvalidateBoneCache( true );
#endif

	m_pWeaponLast = m_pWeapon;
	m_vecPositionLast = m_vecPositionCurrent;
	m_bFirstRunSinceInit = false;
	m_flLastUpdateTime = gpGlobals->curtime;
	m_nLastUpdateFrame = gpGlobals->framecount;
}

float CCSGOPlayerAnimState::LerpCrouchWalkRun( float flCrouchVal, float flWalkVal, float flRunVal )
{
	// lerp between three separate magic numbers intended for each state
	return Lerp( m_flAnimDuckAmount, Lerp( m_flWalkToRunTransition, flWalkVal, flRunVal ), flCrouchVal );
}

#define HYPEREXTENSION_LIMIT 34.2 // The length of a CS player's leg (hip to ankle) when nearly fully extended
#define HYPEREXTENSION_LIMIT_SQR HYPEREXTENSION_LIMIT * HYPEREXTENSION_LIMIT
#define FOOT_SAFE_ZONE_LEFT_SQR 7 * 7
#define FOOT_SAFE_ZONE_RIGHT_SQR 8 * 8
#define ANKLE_HEIGHT 4.5f
#define FOOT_PROXIMITY_LIMIT_SQR 12 * 12 // don't let feet come to rest this close together
#define LATERAL_BLENDOUT_HEIGHT 1
#define LATERAL_BLENDIN_TIME 20
#define LATERAL_BLENDOUT_TIME 6


#define FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN 4.0f
#define FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX 10.0f
#ifdef CLIENT_DLL
extern ConVar cl_camera_height_restriction_debug;
#endif
void CCSGOPlayerAnimState::ModifyEyePosition( Vector& vecInputEyePos )
{
	if ( !m_pPlayer )
		return;
	
#ifdef CLIENT_DLL
	if ( IsPreCrouchUpdateDemo() )
		return;
#endif

	// The local player sets up their third-person bones to locate the position of their head,
	// then this position is used to softly bound the vertical camera position for the client.

	if ( !m_bLanding && m_flAnimDuckAmount == 0 )
	{
#ifdef CLIENT_DLL
		m_bSmoothHeightValid = false;
#endif
		return;
	}

	int nHeadBone = m_pPlayer->LookupBone( "head_0" );
	if ( nHeadBone != -1 )
	{
		Vector vecHeadPos;
		QAngle temp;
		m_pPlayer->GetBonePosition( nHeadBone, vecHeadPos, temp );
		vecHeadPos.z += 1.7f;

#ifdef CLIENT_DLL
		if ( cl_camera_height_restriction_debug.GetBool() )
		{
			Vector vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z + FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 255, 0, 255, true, gpGlobals->frametime );
			vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z + FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 0, 0, 255, true, gpGlobals->frametime );
			vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecInputEyePos.z );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 255, 0, 0, true, gpGlobals->frametime );
			//vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z - FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN );
			//debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 0, 0, 255, true, gpGlobals->frametime );
			//vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecHeadPos.z - FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX );
			//debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 255, 0, 255, true, gpGlobals->frametime );
		}
#endif

		// Only correct the eye if the camera is ABOVE the head. If the camera is below the head, that's unlikely
		// to be advantageous for the local player.
		if ( vecHeadPos.z < vecInputEyePos.z )
		{
			
			float flLerp = SimpleSplineRemapValClamped( abs( vecInputEyePos.z - vecHeadPos.z ),
				FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MIN,
				FIRSTPERSON_TO_THIRDPERSON_VERTICAL_TOLERANCE_MAX,
				0.0f, 1.0f );

			vecInputEyePos.z = Lerp( flLerp, vecInputEyePos.z, vecHeadPos.z );

		}

#ifdef CLIENT_DLL
		if ( cl_camera_height_restriction_debug.GetBool() )
		{
			Vector vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecInputEyePos.z );
			debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,20,0), 0, 255, 0, true, gpGlobals->frametime );
		}

		// when fully crouched on the client, floor the camera height under the input z within a vertical range so it doesn't 'bob' continuously
		if ( m_flAnimDuckAmount >= 1 )
		{
			if ( m_bSmoothHeightValid )
			{
				float flHardCamHeight = m_pPlayer->GetAbsOrigin().z + VEC_DUCK_VIEW.z;

				if ( vecInputEyePos.z < flHardCamHeight )
				{
					m_flCameraSmoothHeight = clamp( MIN( m_flCameraSmoothHeight, vecInputEyePos.z ), vecInputEyePos.z - clamp( flHardCamHeight - vecInputEyePos.z, 0.0f, 3.0f ), vecInputEyePos.z );
					vecInputEyePos.z = m_flCameraSmoothHeight;
				}
				else
				{
					m_flCameraSmoothHeight = flHardCamHeight;
				}
		
				if ( cl_camera_height_restriction_debug.GetBool() )
				{
					Vector vecTemp = Vector( vecInputEyePos.x, vecInputEyePos.y, vecInputEyePos.z );
					debugoverlay->AddLineOverlay( vecTemp, vecTemp + Vector(0,30,0), 255, 255, 255, true, gpGlobals->frametime );
				}
		
			}
			else
			{
				m_flCameraSmoothHeight = vecInputEyePos.z;
				m_bSmoothHeightValid = true;
			}
		}
		else
		{
			m_bSmoothHeightValid = false;
		}

#endif

	}
}


#ifdef CLIENT_DLL

void CCSGOPlayerAnimState::OnClientWeaponChange( CWeaponCSBase* pCurrentWeapon )
{
	if ( !m_bFirstRunSinceInit && ( pCurrentWeapon != m_pWeaponLastBoneSetup ) &&
		!m_pPlayer->IsAnyBoneSnapshotPending() &&
		m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_UPPER_BODY ].GetCurrentWeight() <= 0 && 
		m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_ENTIRE_BODY ].GetCurrentWeight() <= 0 )
	{
		m_pWeaponLastBoneSetup = pCurrentWeapon;
		if ( m_flSpeedAsPortionOfWalkTopSpeed > 0.25f )
		{
			m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_UPPER_BODY ].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_weaponchange ) );
		}
		else
		{
			m_pPlayer->m_boneSnapshots[ BONESNAPSHOT_ENTIRE_BODY ].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_weaponchange ) );
		}
	}
}

inline bool CCSGOPlayerAnimState::LayerToIndex( const CAnimationLayer* pLayer, int &nIndex )
{
	for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
	{
		if ( pLayer == m_pPlayer->GetAnimOverlay( m_pLayerOrderPreset[i], USE_ANIMLAYER_RAW_INDEX ) )
		{
			nIndex = i;
			return true;
		}
	}
	return false;
}

void CCSGOPlayerAnimState::NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence )
{
	int nIndex;
	if ( !LayerToIndex( pLayer, nIndex ) )
		return;
	animstate_layer_t nLayerIndex = (animstate_layer_t)nIndex;
	
	//todo: more hooks for pre-bonesetup sequence changes

	// bone snapshot land/climb transitions
	if ( !m_bFirstRunSinceInit && nLayerIndex == ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB )
	{
		int nLastSequence = pLayer->GetSequence();
		if ( nLastSequence > 0 && nLastSequence != nNewSequence )
		{
			if ( m_pPlayer->GetSequenceActivity( nLastSequence ) == ACT_CSGO_CLIMB_LADDER )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_ladderexit ) );
			}
			else if ( m_pPlayer->GetSequenceActivity( nNewSequence ) == ACT_CSGO_CLIMB_LADDER )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_ladderenter ) );
			}
		}
	}

}

void CCSGOPlayerAnimState::NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight )
{
	int nIndex;
	if ( !LayerToIndex( pLayer, nIndex ) )
		return;
	animstate_layer_t nLayerIndex = (animstate_layer_t)nIndex;

	//todo: more hooks for pre-bonesetup sequence changes

	if ( nLayerIndex == ANIMATION_LAYER_MOVEMENT_MOVE )
	{
		m_flMoveWeight = flNewWeight;
	}
}

void CCSGOPlayerAnimState::NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewcycle )
{
	int nIndex;
	if ( !LayerToIndex( pLayer, nIndex ) )
		return;
	animstate_layer_t nLayerIndex = (animstate_layer_t)nIndex;

	//todo: more hooks for pre-bonesetup sequence changes
	
	if ( nLayerIndex == ANIMATION_LAYER_MOVEMENT_MOVE )
	{
		m_flPrimaryCycle = flNewcycle;
	}
}

inline float CCSGOPlayerAnimState::FootBarrierEq( float flIn, float flMinWidth )
{
	return (( Sqr(flIn) * 0.02f ) + MIN( flMinWidth, 3 )) * MIN( m_flSpeedAsPortionOfCrouchTopSpeed, 1 );
}

void CCSGOPlayerAnimState::DoProceduralFootPlant( matrix3x4a_t boneToWorld[], mstudioikchain_t *pLeftFootChain, mstudioikchain_t *pRightFootChain, BoneVector pos[] )
{
	if ( !m_pPlayer )
		return;
	
	if ( m_bOnGround && m_flInAirSmoothValue >= 1 && !m_bOnLadder )
	{

		if ( m_pPlayer->GetGroundEntity() && !m_pPlayer->GetGroundEntity()->IsWorld() )
			return;

		int nLeftFootBoneIndex = pLeftFootChain->pLink( 2 )->bone;
		int nRightFootBoneIndex = pRightFootChain->pLink( 2 )->bone;

		int nLeftLockIndex = m_pPlayer->LookupBone( "lfoot_lock" );
		int nRightLockIndex = m_pPlayer->LookupBone( "rfoot_lock" );

		if ( nLeftLockIndex > 0 && nRightLockIndex > 0 )
		{
			m_footLeft.m_vecPosAnim = boneToWorld[nLeftFootBoneIndex].GetOrigin();
			m_footRight.m_vecPosAnim = boneToWorld[nRightFootBoneIndex].GetOrigin();
			
			//debugoverlay->AddBoxOverlay( m_footLeft.m_vecPosAnim, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 255, 0, 0, 0, 0 );
			//debugoverlay->AddBoxOverlay( m_footRight.m_vecPosAnim, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 255, 0, 0, 0, 0 );

			Vector vecPlayerOrigin = m_pPlayer->GetAbsOrigin();
			
			// reset the procedural locations if this is the first computation or they haven't been computed recently
			if ( m_bFirstFootPlantSinceInit || abs(gpGlobals->framecount - m_iLastUpdateFrame) > 10 )
			{
				m_footLeft.Init( m_footLeft.m_vecPosAnim );
				m_footRight.Init( m_footRight.m_vecPosAnim );

				m_footLeft.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
				m_footRight.m_vecPlantVel = m_vecVelocityNormalizedNonZero;

				m_iLastUpdateFrame = gpGlobals->framecount;
				m_bFirstFootPlantSinceInit = false;
				return;
			}
			
			// use the ik lock driver bones to determine if a new foot plant location has been triggered

			#define FOOT_LOCK_THRESHOLD 0.7f
			
			bool bLeftFootLockWasBelowThreshold = m_footLeft.m_flLockAmount < FOOT_LOCK_THRESHOLD;
			bool bRightFootLockWasBelowThreshold = m_footRight.m_flLockAmount < FOOT_LOCK_THRESHOLD;
			
			m_footLeft.m_flLockAmount = Approach( clamp( abs(pos[nLeftLockIndex].y), 0, 1 ), m_footLeft.m_flLockAmount, gpGlobals->frametime * 10.0f );
			m_footRight.m_flLockAmount = Approach( clamp( abs(pos[nRightLockIndex].y), 0, 1 ), m_footRight.m_flLockAmount, gpGlobals->frametime * 10.0f );
			
			bool bLeftFootLockIsAboveThreshold = m_footLeft.m_flLockAmount >= FOOT_LOCK_THRESHOLD;
			bool bRightFootLockIsAboveThreshold = m_footRight.m_flLockAmount >= FOOT_LOCK_THRESHOLD;
			
			// for a short duration after each foot-plant, the feet are not allowed to deviate laterally from the player velocity at the time of the plant

			float flLeftTimeLerp = RemapValClamped( m_footLeft.m_flLastPlantTime, gpGlobals->curtime, gpGlobals->curtime-0.4f, 1.0f, 0.0f );
			float flRightTimeLerp = RemapValClamped( m_footRight.m_flLastPlantTime, gpGlobals->curtime, gpGlobals->curtime-0.4f, 1.0f, 0.0f );
			
			flLeftTimeLerp = Gain( flLeftTimeLerp, 0.8f );
			flRightTimeLerp = Gain( flRightTimeLerp, 0.8f );
			
			//debugoverlay->AddBoxOverlay( m_footLeft.m_vecPosPlant, Vector(-5,-5,0)*flLeftTimeLerp, Vector(5,5,0)*flLeftTimeLerp, QAngle(0,0,0), 0, 255, 255, 0, 0 );
			//debugoverlay->AddBoxOverlay( m_footRight.m_vecPosPlant, Vector(-5,-5,0)*flRightTimeLerp, Vector(5,5,0)*flRightTimeLerp, QAngle(0,0,0), 255, 255, 0, 0, 0 );
			
			Vector vecLeftPtOnVelLine;
			Vector vecRightPtOnVelLine;

			CalcClosestPointOnLine( m_footLeft.m_vecPosAnim, m_footLeft.m_vecPosPlant, m_footLeft.m_vecPosPlant + m_footLeft.m_vecPlantVel, vecLeftPtOnVelLine );
			CalcClosestPointOnLine( m_footRight.m_vecPosAnim, m_footRight.m_vecPosPlant, m_footRight.m_vecPosPlant + m_footRight.m_vecPlantVel, vecRightPtOnVelLine );
			
			Vector vecLeftTarget = Lerp( flLeftTimeLerp, m_footLeft.m_vecPosAnim, vecLeftPtOnVelLine );
			Vector vecRightTarget = Lerp( flRightTimeLerp, m_footRight.m_vecPosAnim, vecRightPtOnVelLine );
			

			// check for foot plants for next frame

			if ( bLeftFootLockIsAboveThreshold && bLeftFootLockWasBelowThreshold )
			{
				m_footLeft.m_vecPosPlant = vecLeftTarget;
				m_footLeft.m_flLastPlantTime = gpGlobals->curtime;
				m_footLeft.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
			}
			
			if ( bRightFootLockIsAboveThreshold && bRightFootLockWasBelowThreshold )
			{
				m_footRight.m_vecPosPlant = vecRightTarget;
				m_footRight.m_flLastPlantTime = gpGlobals->curtime;
				m_footRight.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
			}

			// always inherit animated z (only foot-stepping below modifies this)
			vecLeftTarget.z = m_footLeft.m_vecPosAnim.z;
			vecRightTarget.z = m_footRight.m_vecPosAnim.z;


			// the feet are not allowed to exceed 3x the instantaneous velocity of the player (prevents pose-popping when mashing keys)

			float flFrametime = MAX( gpGlobals->frametime, 0.0001f );
			float flLeftDeltaVel = MAX( ( m_footLeft.m_vecPosAnimLast - vecLeftTarget ).Length() / flFrametime, 0.0001f );
			float flRightDeltaVel = MAX( ( m_footRight.m_vecPosAnimLast - vecRightTarget ).Length() / flFrametime, 0.0001f );

			float flVelLimitLeft = m_flVelocityLengthXY * 3.0f;
			float flVelLimitRight = m_flVelocityLengthXY * 3.0f;

			if ( m_flVelocityLengthXY > 10.0f )
			{
				m_flLastTimeVelocityOverTen = gpGlobals->curtime;
			}
			float flTimeStill = gpGlobals->curtime - m_flLastTimeVelocityOverTen;

			// when standing mostly still, alternately raise the velocity limit floor, which makes the feet 'shuffle-step'
			if ( m_flVelocityLengthXY <= 10.0f )
			{

				float flEyeFootAngleDiff = abs( AngleDiff(m_flEyeYaw, m_flFootYaw) );
				if ( flEyeFootAngleDiff > 56.0f || flTimeStill < 1.0f ) // when turning rapidly, allow the feet to step faster
				{
					float flFmod = fmod( gpGlobals->curtime, 0.33f );
					if ( flFmod < 0.16f )
					{
						flVelLimitLeft = 110.0f;
					}
					else if ( flFmod >= 0.17f )
					{
						flVelLimitRight = 130.0f;
					}
				}
				else
				{
					float flFmod = fmod( gpGlobals->curtime, 0.66f );
					if ( flFmod > 0.02 && flFmod < 0.31f )
					{
						flVelLimitLeft = 80.0f;
					}
					else if ( flFmod > 0.35 && flFmod < 0.64f )
					{
						flVelLimitRight = 70.0f;
					}
				}

				// less fudge-factor when crouching, so allow the feet to catch up faster
				if ( m_flAnimDuckAmount > 0.5f )
				{
					flVelLimitLeft *= 2.0f;
					flVelLimitRight *= 2.0f;
				}

				// also lift the feet a bit on their way to their target
				//if ( flVelLimitLeft > 0 )
				//{
				//	float flDistToLeft = RemapValClamped( m_footLeft.m_vecPosAnimLast.DistTo( m_footLeft.m_vecPosAnim ), 0.0f, 30.0f, 0.0f, 1.0f );
				//	vecLeftTarget.z += SmoothCurve( flDistToLeft ) * 14.0f;
				//}
				//if ( flVelLimitRight> 0 )
				//{
				//	float flDistToRight = RemapValClamped( m_footRight.m_vecPosAnimLast.DistTo( m_footRight.m_vecPosAnim ), 0.0f, 30.0f, 0.0f, 1.0f );
				//	vecRightTarget.z += SmoothCurve( flDistToRight ) * 14.0f;
				//}

			}

			// restrict foot velocity

			if ( flLeftDeltaVel > flVelLimitLeft )
				vecLeftTarget = Lerp( flVelLimitLeft / flLeftDeltaVel, m_footLeft.m_vecPosAnimLast, vecLeftTarget );

			if ( flRightDeltaVel > flVelLimitRight )
				vecRightTarget = Lerp( flVelLimitRight / flRightDeltaVel, m_footRight.m_vecPosAnimLast, vecRightTarget );


			// spawn oddities like lowering the player artificially without allowing them to fall can move the player
			// but it doesn't count as a teleport, so their velocity remains zero... long story short this causes the 
			// target z values to catch up instead of reset and it looks weird. I'm clamping their range here:
			vecLeftTarget.z = clamp( vecLeftTarget.z, m_footLeft.m_vecPosAnim.z - 2.0f, m_footLeft.m_vecPosAnim.z + 6.0f );
			vecRightTarget.z = clamp( vecRightTarget.z, m_footRight.m_vecPosAnim.z - 2.0f, m_footRight.m_vecPosAnim.z + 6.0f );


			// sanity-check the result and throw out the positions if they're super weird
			// (we might have been teleported or pvs went nuts or there was a bunch of packet loss - 
			// point being it's easier and more reliable to error check than try to prevent the input cases.

			int nLeftHipBoneIndex = pLeftFootChain->pLink( 0 )->bone;
			int nRightHipBoneIndex = pRightFootChain->pLink( 0 )->bone;
			if ( nLeftHipBoneIndex > 0 && nRightHipBoneIndex > 0 )
			{
				if ( boneToWorld[nLeftHipBoneIndex].GetOrigin().DistToSqr( vecLeftTarget ) > 1400 ||
					 boneToWorld[nRightHipBoneIndex].GetOrigin().DistToSqr( vecRightTarget ) > 1400 )
				{

					//debugoverlay->AddLineOverlay( boneToWorld[nLeftHipBoneIndex].GetOrigin(), vecLeftTarget, 255,0,0, true, 5 );
					//debugoverlay->AddLineOverlay( boneToWorld[nRightHipBoneIndex].GetOrigin(), vecRightTarget, 255,0,0, true, 5 );

					// if either foot is way out of range, bail and reset them both
					m_footLeft.Init( m_footLeft.m_vecPosAnim );
					m_footRight.Init( m_footRight.m_vecPosAnim );

					m_footLeft.m_vecPlantVel = m_vecVelocityNormalizedNonZero;
					m_footRight.m_vecPlantVel = m_vecVelocityNormalizedNonZero;

					m_iLastUpdateFrame = gpGlobals->framecount;
					m_bFirstFootPlantSinceInit = false;
					return;
				}
			}

			if ( vecLeftTarget.DistToSqr( m_footLeft.m_vecPosAnim ) > (12 * 12) )
			{
				vecLeftTarget = m_footLeft.m_vecPosAnim + (( vecLeftTarget - m_footLeft.m_vecPosAnim ).Normalized() * 12.0f);
				//debugoverlay->AddLineOverlay( vecLeftTarget, m_footLeft.m_vecPosAnim, 0, 255, 0, true, 0 );
			}

			if ( vecRightTarget.DistToSqr( m_footRight.m_vecPosAnim ) > (12 * 12) )
			{
				vecRightTarget = m_footRight.m_vecPosAnim + ((vecRightTarget - m_footRight.m_vecPosAnim).Normalized() * 12.0f);
				//debugoverlay->AddLineOverlay( vecRightTarget, m_footRight.m_vecPosAnim, 0, 255, 0, true, 0 );
			}

			// place the foot bones at the newly computed locations (ik will solve the knees)

			boneToWorld[nLeftFootBoneIndex].SetOrigin( vecLeftTarget );
			boneToWorld[nRightFootBoneIndex].SetOrigin( vecRightTarget );
			
			//debugoverlay->AddBoxOverlay( vecLeftTarget, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 0, 255, 0, 0, 0 );
			//debugoverlay->AddBoxOverlay(vecRightTarget, Vector(-5,-5,0), Vector(5,5,0), QAngle(0,0,0), 0, 255, 0, 0, 0 );

			m_footLeft.m_vecPosAnimLast = vecLeftTarget;
			m_footRight.m_vecPosAnimLast = vecRightTarget;

		}

		if ( m_iLastUpdateFrame < gpGlobals->framecount )
		{
			m_iLastUpdateFrame = gpGlobals->framecount;
		}
	}	
}
#endif

void CCSGOPlayerAnimState::SetUpLean( void )
{
	// lean the body into velocity derivative (acceleration) to simulate maintaining a center of gravity
	float flInterval = gpGlobals->curtime - m_flLastVelocityTestTime;
	if ( flInterval > 0.025f )
	{
		flInterval = MIN( flInterval, 0.1f );
		m_flLastVelocityTestTime = gpGlobals->curtime;

		m_vecTargetAcceleration = ( m_pPlayer->GetLocalVelocity() - m_vecVelocityLast ) / flInterval;
		m_vecTargetAcceleration.z = 0;

		m_vecVelocityLast = m_pPlayer->GetLocalVelocity();
	}

	m_vecAcceleration = Approach( m_vecTargetAcceleration, m_vecAcceleration, m_flLastUpdateIncrement * 800.0f );

//#ifdef CLIENT_DLL
//	debugoverlay->AddLineOverlay( m_vecPositionCurrent, m_vecPositionCurrent + m_vecAcceleration, 255,0,0, 255, 1, m_flLastUpdateIncrement );
//#else
//	debugoverlay->AddLineOverlay( m_vecPositionCurrent, m_vecPositionCurrent + m_vecAcceleration, 0,0,255, 255, 1.5, m_flLastUpdateIncrement );
//#endif

	QAngle temp;
	VectorAngles( m_vecAcceleration, Vector(0,0,1), temp );
	
	m_flAccelerationWeight = clamp( (m_vecAcceleration.Length() / CS_PLAYER_SPEED_RUN) * m_flSpeedAsPortionOfRunTopSpeed, 0, 1 );
	m_flAccelerationWeight *= (1.0f - m_flLadderWeight);

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_LEAN_YAW ].SetValue( m_pPlayer, AngleNormalize( m_flFootYaw - temp[YAW] ) );

	if ( GetLayerSequence( ANIMATION_LAYER_LEAN ) <= 0 )
	{
		MDLCACHE_CRITICAL_SECTION();
		SetLayerSequence( ANIMATION_LAYER_LEAN, m_pPlayer->LookupSequence( "lean" ) );
	}

	SetLayerWeight( ANIMATION_LAYER_LEAN, m_flAccelerationWeight );
}

void CCSGOPlayerAnimState::SetUpFlinch( void )
{
#ifndef CLIENT_DLL
	if ( m_flTimeOfLastKnownInjury < m_pPlayer->GetTimeOfLastInjury() )
	{
		m_flTimeOfLastKnownInjury = m_pPlayer->GetTimeOfLastInjury();

		// flinches override flinches of their own priority
		bool bNoFlinchIsPlaying = ( IsLayerSequenceCompleted( ANIMATION_LAYER_FLINCH ) || GetLayerWeight( ANIMATION_LAYER_FLINCH ) <= 0 );
		bool bHeadshotIsPlaying = ( !bNoFlinchIsPlaying && GetLayerActivity(ANIMATION_LAYER_FLINCH) == ACT_CSGO_FLINCH_HEAD );

		if ( m_pPlayer->GetLastDamageTypeFlags() & DMG_BURN )
		{
			if ( bNoFlinchIsPlaying )
			{
				SetLayerSequence( ANIMATION_LAYER_FLINCH, SelectSequenceFromActMods( ACT_CSGO_FLINCH_MOLOTOV ) );
			
				// clear out all the flinch-related actmods now we selected a sequence
				UpdateActivityModifiers();
			}
		}
		else if ( bNoFlinchIsPlaying || !bHeadshotIsPlaying || m_pPlayer->LastHitGroup() == HITGROUP_HEAD )
		{
			RelativeDamagedDirection_t damageDir = m_pPlayer->GetLastInjuryRelativeDirection();
			bool bLeft = false;
			bool bRight = false;
			switch (damageDir) {
				case DAMAGED_DIR_NONE:
				case DAMAGED_DIR_FRONT:
				{
					AddActivityModifier( "front" );
					break;
				}
				case DAMAGED_DIR_BACK:
				{
					AddActivityModifier( "back" );
					break;
				}
				case DAMAGED_DIR_LEFT:
				{
					AddActivityModifier( "left" );
					bLeft = true;
					break;
				}
				case DAMAGED_DIR_RIGHT:
				{
					AddActivityModifier( "right" );
					bRight = true;
					break;
				}
			}
			switch (m_pPlayer->LastHitGroup()) {
				case HITGROUP_HEAD:
				{
					AddActivityModifier( "head" );
					break;
				}
				case HITGROUP_CHEST:
				{
					AddActivityModifier( "chest" );
					break;
				}
				case HITGROUP_LEFTARM:
				{
					if ( !bLeft ) { AddActivityModifier( "left" ); }
					AddActivityModifier( "arm" );
					break;
				}
				case HITGROUP_RIGHTARM:
				{
					if ( !bRight ) { AddActivityModifier( "right" ); }
					AddActivityModifier( "arm" );
					break;
				}
				case HITGROUP_GENERIC:
				case HITGROUP_STOMACH:
				{
					AddActivityModifier( "gut" );
					break;
				}
				case HITGROUP_LEFTLEG:
				{
					if ( !bLeft ) { AddActivityModifier( "left" ); }
					AddActivityModifier( "leg" );
					break;
				}
				case HITGROUP_RIGHTLEG:
				{
					if ( !bRight ) { AddActivityModifier( "right" ); }
					AddActivityModifier( "leg" );
					break;
				}
			}
			SetLayerSequence( ANIMATION_LAYER_FLINCH, SelectSequenceFromActMods( (m_pPlayer->LastHitGroup() == HITGROUP_HEAD) ? ACT_CSGO_FLINCH_HEAD : ACT_CSGO_FLINCH ) );
			
			// clear out all the flinch-related actmods now we selected a sequence
			UpdateActivityModifiers();
		}

	}

	if ( GetLayerSequence( ANIMATION_LAYER_FLINCH ) > 0 )
	{
		SetLayerWeight( ANIMATION_LAYER_FLINCH, GetLayerIdealWeightFromSeqCycle( ANIMATION_LAYER_FLINCH ) );
	}
	else
	{
		SetLayerWeight( ANIMATION_LAYER_FLINCH, 0 );
	}

#endif

	IncrementLayerCycle( ANIMATION_LAYER_FLINCH, false );
}

void CCSGOPlayerAnimState::SetUpFlashedReaction( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_FLASHED;

#ifndef CLIENT_DLL

	if ( m_flFlashedAmountEaseOutEnd < gpGlobals->curtime )
	{
		SetLayerWeight( nLayer, 0 );
		m_bFlashed = false;
	}
	else
	{

		if ( !m_bFlashed )
		{
			SetLayerSequence( nLayer, SelectSequenceFromActMods( ACT_CSGO_FLASHBANG_REACTION ) );
			m_bFlashed = true;
		}

		float flFlashedAmount = RemapValClamped( gpGlobals->curtime, m_flFlashedAmountEaseOutStart, m_flFlashedAmountEaseOutEnd, 1, 0 );
		
		// TODO: make flashed anims look nicer by using a cycle, like the old ones
		//SetLayerCycle( nLayer, 1.0f - flFlashedAmount );

		SetLayerCycle( nLayer, 0 );
		SetLayerRate( nLayer, 0 );

		float flWeightPrevious = GetLayerWeight( nLayer );
		float flWeightNew = SimpleSpline(flFlashedAmount);

		SetLayerWeight( nLayer, flWeightNew );
		SetLayerWeightRate( nLayer, (flWeightNew >= flWeightPrevious) ? 0 : flWeightPrevious );
	}

#else

	if ( GetLayerWeight( nLayer ) > 0 )
	{
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayer, USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer && pLayer->GetWeightDeltaRate() < 0 )
			IncrementLayerWeight( nLayer );
	}

#endif
}

#define CSGO_ANIM_BOMBPLANT_ABORT_SPEED 12.0f
#define CSGO_ANIM_DEFUSE_ABORT_SPEED 8.0f
#define CSGO_ANIM_TWITCH_ABORT_SPEED 6.0f
#define CSGO_ANIM_BOMBPLANT_BLEND_RATE 1.2f
void CCSGOPlayerAnimState::SetUpWholeBodyAction( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_WHOLE_BODY;

#ifndef CLIENT_DLL

	// Whole body anims are for custom events that typically take over the entire body of the player.
	// At the moment, they're used for idle twitches in freeze time, defusing and bomb-planting,
	// and probably taunts in the near future.


	//if ( !m_bDefuseStarted &&
	//	 !m_bPlantAnimStarted &&
	//	 m_flNextTwitchTime <= gpGlobals->curtime &&
	//	 AreTwitchesAllowed() )
	//{
	//	// roll for a twitch anim
	//	bool bFirstInit = ( m_flNextTwitchTime == 0 );
	//	m_flNextTwitchTime = gpGlobals->curtime + RandomFloat( 8, 16 );
	//	
	//	if ( !bFirstInit && !m_bTwitchAnimStarted && m_bOnGround )
	//	{
	//		// start the twitch anim 
	//		SetLayerSequence( nLayer, SelectSequenceFromActMods( ACT_CSGO_TWITCH_BUYZONE ) );
	//		m_bTwitchAnimStarted = true;
	//	}
	//}


	if ( m_pPlayer->GetTeamNumber() == TEAM_CT && m_pPlayer->m_bIsDefusing ) // should be defusing
	{
		if ( !m_bDefuseStarted )
		{
			// we are now defusing and need to start the anim
			SetLayerSequence( nLayer, SelectSequenceFromActMods( m_pPlayer->HasDefuser() ? ACT_CSGO_DEFUSE_WITH_KIT : ACT_CSGO_DEFUSE ) );
			m_bDefuseStarted = true;
		}
		else
		{
			IncrementLayerCycleWeightRateGeneric(nLayer);
		}
	}
	else if ( GetLayerActivity( nLayer ) == ACT_CSGO_DEFUSE || GetLayerActivity( nLayer ) == ACT_CSGO_DEFUSE_WITH_KIT )
	{
		// should NOT be defusing but IS
		if ( GetLayerWeight( nLayer ) > 0 )
		{
			float flCurrentWeight = GetLayerWeight( nLayer );
			SetLayerWeight( nLayer, Approach( 0, flCurrentWeight, m_flLastUpdateIncrement * CSGO_ANIM_DEFUSE_ABORT_SPEED ) );
			SetLayerWeightRate( nLayer, flCurrentWeight );
		}
		m_bDefuseStarted = false;
	}
	else if ( GetLayerActivity( nLayer ) == ACT_CSGO_PLANT_BOMB ) // is planting
	{
		if ( m_pWeapon && !m_pWeapon->IsA( WEAPON_C4 ) )
			m_bPlantAnimStarted = false; // cancel planting if we're not holding c4

		if ( m_bPlantAnimStarted ) // plant in progress
		{
			// Inlined IncrementLayerCycleWeightRateGeneric() so we can tune the layering weight when crouch-planting the bomb.
			//
			// Instead of setting the weight to GetLayerIdealWeightFromSeqCycle, we approach that value which smoothly blends to
			// the bomb plant animation.  This means that the 'standing' part of the animation doesn't get overly blended to when
			// planting the animation from a crouched position.  In addition, if you are in thirdperson camera, the crouch is
			// predicted but the plant animation is server-side.  So we do this blend regardless of the crouch state because it
			// could differ between the client and the server.
			//
			// This does mean that fine detail in the beginning of the plant animation is lost.  Fortunately there isn't much of
			// that at the moment.
			float flWeightPrevious = GetLayerWeight( nLayer );
			IncrementLayerCycle( nLayer, false );
			SetLayerWeight( nLayer, Approach( GetLayerIdealWeightFromSeqCycle( nLayer ), flWeightPrevious, m_flLastUpdateIncrement * CSGO_ANIM_BOMBPLANT_BLEND_RATE ) );
			SetLayerWeightRate( nLayer, flWeightPrevious );

			m_bPlantAnimStarted = !( IsLayerSequenceCompleted( nLayer ) );
		}
		else
		{
			if ( GetLayerWeight( nLayer ) > 0 )
			{
				//bomb plant aborted, pull out the weight
				float flCurrentWeight = GetLayerWeight( nLayer );
				SetLayerWeight( nLayer, Approach( 0, flCurrentWeight, m_flLastUpdateIncrement * CSGO_ANIM_BOMBPLANT_ABORT_SPEED ) );
				SetLayerWeightRate( nLayer, flCurrentWeight );
			}
			m_bPlantAnimStarted = false;
		}
	}
	//else if ( GetLayerActivity( nLayer ) == ACT_CSGO_TWITCH_BUYZONE || GetLayerActivity( nLayer ) == ACT_CSGO_TWITCH ) // twitching
	//{
	//	if ( m_pWeapon && m_pWeapon != m_pWeaponLast )
	//		m_bTwitchAnimStarted = false; // cancel twitches if weapon changes
	//
	//	if ( m_bTwitchAnimStarted && AreTwitchesAllowed() )
	//	{
	//		IncrementLayerCycleWeightRateGeneric(nLayer);
	//		m_bTwitchAnimStarted = !( IsLayerSequenceCompleted( nLayer ) );
	//	}
	//	else
	//	{
	//		if ( GetLayerWeight( nLayer ) > 0 )
	//		{
	//			float flCurrentWeight = GetLayerWeight( nLayer );
	//			SetLayerWeight( nLayer, Approach( 0, flCurrentWeight, m_flLastUpdateIncrement * CSGO_ANIM_TWITCH_ABORT_SPEED ) );
	//			SetLayerWeightRate( nLayer, flCurrentWeight );
	//		}
	//		m_bTwitchAnimStarted = false;
	//	}
	//}
	else
	{
		// fallback
		SetLayerCycle( nLayer, 0.999f );
		SetLayerWeight( nLayer, 0 );
		SetLayerWeightRate( nLayer, 0 );
	}

#else

	if ( GetLayerWeight( nLayer ) > 0 )
	{
		IncrementLayerCycle( nLayer, false );
		IncrementLayerWeight( nLayer );
	}

#endif

}

void CCSGOPlayerAnimState::SetUpAliveloop( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_ALIVELOOP;

#ifndef CLIENT_DLL
	if ( GetLayerActivity( nLayer ) != ACT_CSGO_ALIVE_LOOP )
	{
		// first time init
		MDLCACHE_CRITICAL_SECTION();
		SetLayerSequence( nLayer, SelectSequenceFromActMods( ACT_CSGO_ALIVE_LOOP ) );
		SetLayerCycle( nLayer, RandomFloat( 0, 1 ) );
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayer, USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer )
		{
			float flNewRate = m_pPlayer->GetSequenceCycleRate( m_pPlayer->GetModelPtr(), pLayer->GetSequence() );
			flNewRate *= RandomFloat( 0.8f, 1.1f );
			SetLayerRate( nLayer, flNewRate );
		}
	}
	else
	{
		if ( m_pWeapon && m_pWeapon != m_pWeaponLast )
		{
			//re-roll act on weapon change
			float flRetainCycle = GetLayerCycle( nLayer );
			SetLayerSequence( nLayer, SelectSequenceFromActMods( ACT_CSGO_ALIVE_LOOP ) );
			SetLayerCycle( nLayer, flRetainCycle );
		}
		else if ( IsLayerSequenceCompleted( nLayer ) )
		{
			//re-roll rate
			MDLCACHE_CRITICAL_SECTION();
			CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayer, USE_ANIMLAYER_RAW_INDEX );
			if ( pLayer )
			{
				float flNewRate = m_pPlayer->GetSequenceCycleRate( m_pPlayer->GetModelPtr(), pLayer->GetSequence() );
				flNewRate *= RandomFloat( 0.8f, 1.1f );
				SetLayerRate( nLayer, flNewRate );
			}
		}
		else
		{
			float flWeightOutPoseBreaker = RemapValClamped( m_flSpeedAsPortionOfRunTopSpeed, 0.55f, 0.9f, 1.0f, 0.0f );
			SetLayerWeight( nLayer, flWeightOutPoseBreaker );
		}
	}
#endif

	IncrementLayerCycle( nLayer, true );
	//SetLayerWeight( nLayer, 1 );
}

void CCSGOPlayerAnimState::SetUpWeaponAction( void )
{
	animstate_layer_t nLayer = ANIMATION_LAYER_WEAPON_ACTION;
	
#ifndef CLIENT_DLL

	bool bDoIncrement = true;

	if ( m_pWeapon && m_bDeployRateLimiting && GetLayerActivity( nLayer ) == ACT_CSGO_DEPLOY )
	{
		m_pWeapon->ShowWeaponWorldModel( false );

		if ( GetLayerCycle( nLayer ) >= CSGO_ANIM_DEPLOY_RATELIMIT )
		{
			m_bDeployRateLimiting = false;
			SetLayerSequence( nLayer, SelectSequenceFromActMods( ACT_CSGO_DEPLOY ) );
			//SetLayerRate( nLayer, GetLayerRate( nLayer ) * 1.5f );
			SetLayerWeight( nLayer, 0 );
			bDoIncrement = false;
		}
	}

	// fixme: this is a hack to fix all-body weapon actions that need to transition into crouch or stand poses they weren't built for.
	// This only matters for idle animation - the move layer is itself a kind of 're-crouch' and 're-stand' layer itself.

	if ( m_nAnimstateModelVersion < 2 )
	{
		// old re-crouch behavior

		// fixme: this is a hack to fix the all-body weapon action that wants to crouch case. There's no fix for the crouching all-body action that wants to stand
		if ( m_flAnimDuckAmount > 0 && GetLayerWeight(nLayer) > 0 && !LayerSequenceHasActMod( nLayer, "crouch" ) )
		{
			if ( GetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH ) <= 0 )
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH, m_pPlayer->LookupSequence( "recrouch_generic" ) );
			SetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH, GetLayerWeight(nLayer) * m_flAnimDuckAmount );
		}
		else
		{
			SetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH, 0 );
		}
	}
	else
	{
		// newer version with "re-stand" blended into the re-crouch.

		float flTargetRecrouchWeight = 0;
		if ( GetLayerWeight(nLayer) > 0 )
		{
			if ( GetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH ) <= 0 )
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH, m_pPlayer->LookupSequence( "recrouch_generic" ) );

			if ( LayerSequenceHasActMod( nLayer, "crouch" ) )
			{
				// this is a crouching anim. It might be the only anim available, or it's just lasted long enough that the 
				// player stood up after it started. If we're standing up at all, we need to force the stand pose artificially.
				if ( m_flAnimDuckAmount < 1 )
					flTargetRecrouchWeight = GetLayerWeight(nLayer) * (1.0f - m_flAnimDuckAmount);
			}
			else
			{
				// this is NOT a crouching anim. Still if it's not a whole-body anim it might work fine when crouched though, 
				// and not actually need the re-crouch. How to detect this?

				// We can't trust this anim to crouch the player since it's not tagged as a crouch anim. So we need to force the
				// crouch pose artificially.
				if ( m_flAnimDuckAmount > 0 )
					flTargetRecrouchWeight = GetLayerWeight(nLayer) * m_flAnimDuckAmount;
			}
		}
		else
		{
			if ( GetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH ) > 0 )
				flTargetRecrouchWeight = Approach( 0, GetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH ), m_flLastUpdateIncrement * 4 );	
		}
		SetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION_RECROUCH, flTargetRecrouchWeight );
	}

	if ( bDoIncrement )
	{
		// increment the action
		IncrementLayerCycle( nLayer, false );
		float flWeightPrev = GetLayerWeight( nLayer );
		float flDesiredWeight = GetLayerIdealWeightFromSeqCycle( nLayer );

		SetLayerWeight( nLayer, flDesiredWeight );
		SetLayerWeightRate( nLayer, flWeightPrev );
	}

#else

	// client

	if ( GetLayerWeight( nLayer ) > 0 )
	{
		IncrementLayerCycle( nLayer, false );
		IncrementLayerWeight( nLayer );
	}

#endif

	// set weapon sequence and cycle so dispatched events hit
	CAnimationLayer *pWeaponLayer = m_pPlayer->GetAnimOverlay( nLayer, USE_ANIMLAYER_RAW_INDEX );
	if ( pWeaponLayer && m_pWeapon )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = m_pWeapon->m_hWeaponWorldModel.Get();
		if ( pWeaponWorldModel )
		{
			MDLCACHE_CRITICAL_SECTION();
			if ( pWeaponLayer->m_nDispatchedDst > 0 && pWeaponLayer->GetWeight() > 0 ) // fixme: does the weight check make 0-frame events fail? Added a check below just in case.
			{
				pWeaponWorldModel->SetSequence( pWeaponLayer->m_nDispatchedDst );
				pWeaponWorldModel->SetCycle( pWeaponLayer->GetCycle() );
				pWeaponWorldModel->SetPlaybackRate( pWeaponLayer->GetPlaybackRate() );
				#ifndef CLIENT_DLL
				pWeaponWorldModel->DispatchAnimEvents( pWeaponWorldModel );
				#endif
			}
			else
			{
				#ifndef CLIENT_DLL
				if ( pWeaponWorldModel->GetCycle() != 0 )
					pWeaponWorldModel->DispatchAnimEvents( pWeaponWorldModel );
				#endif
				pWeaponWorldModel->SetSequence( 0 );
				pWeaponWorldModel->SetCycle( 0 );
				pWeaponWorldModel->SetPlaybackRate( 0 );
			}
		}
	}
}

#define CSGO_ANIM_WALK_TO_RUN_TRANSITION_SPEED 2.0f
#define CSGO_ANIM_ONGROUND_FUZZY_APPROACH 8.0f
#define CSGO_ANIM_ONGROUND_FUZZY_APPROACH_CROUCH 16.0f
#define CSGO_ANIM_LADDER_CLIMB_COVERAGE 100.0f
#define CSGO_ANIM_RUN_ANIM_PLAYBACK_MULTIPLIER 0.85f
void CCSGOPlayerAnimState::SetUpMovement( void )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( m_flWalkToRunTransition > 0 && m_flWalkToRunTransition < 1 )
	{
		//currently transitioning between walk and run
		if ( m_bWalkToRunTransitionState == ANIM_TRANSITION_WALK_TO_RUN )
		{
			m_flWalkToRunTransition += m_flLastUpdateIncrement * CSGO_ANIM_WALK_TO_RUN_TRANSITION_SPEED;
		}
		else // m_bWalkToRunTransitionState == ANIM_TRANSITION_RUN_TO_WALK
		{
			m_flWalkToRunTransition -= m_flLastUpdateIncrement * CSGO_ANIM_WALK_TO_RUN_TRANSITION_SPEED;
		}
		m_flWalkToRunTransition = clamp( m_flWalkToRunTransition, 0, 1 );
	}

	if ( m_flVelocityLengthXY > (CS_PLAYER_SPEED_RUN*CS_PLAYER_SPEED_WALK_MODIFIER) && m_bWalkToRunTransitionState == ANIM_TRANSITION_RUN_TO_WALK )
	{
		//crossed the walk to run threshold
		m_bWalkToRunTransitionState = ANIM_TRANSITION_WALK_TO_RUN;
		m_flWalkToRunTransition = MAX( 0.01f, m_flWalkToRunTransition );
	}
	else if ( m_flVelocityLengthXY < (CS_PLAYER_SPEED_RUN*CS_PLAYER_SPEED_WALK_MODIFIER) && m_bWalkToRunTransitionState == ANIM_TRANSITION_WALK_TO_RUN )
	{
		//crossed the run to walk threshold
		m_bWalkToRunTransitionState = ANIM_TRANSITION_RUN_TO_WALK;
		m_flWalkToRunTransition = MIN( 0.99f, m_flWalkToRunTransition );
	}

	if ( m_nAnimstateModelVersion < 2 )
	{
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_RUN ].SetValue( m_pPlayer, m_flWalkToRunTransition );
	}
	else
	{
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_WALK ].SetValue( m_pPlayer, (1.0f - m_flWalkToRunTransition) * (1.0f - m_flAnimDuckAmount) );
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_RUN ].SetValue( m_pPlayer, (m_flWalkToRunTransition) * (1.0f - m_flAnimDuckAmount) );
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_CROUCH_WALK ].SetValue( m_pPlayer, m_flAnimDuckAmount );
	}

	char szWeaponMoveSeq[MAX_ANIMSTATE_ANIMNAME_CHARS];
	V_sprintf_safe( szWeaponMoveSeq, "move_%s", GetWeaponPrefix() );

	int nWeaponMoveSeq = m_pPlayer->LookupSequence( szWeaponMoveSeq );
	if ( nWeaponMoveSeq == -1 )
	{
		nWeaponMoveSeq = m_pPlayer->LookupSequence( "move" );
	}
	Assert( nWeaponMoveSeq > 0 );

	
	if ( m_pPlayer->m_iMoveState != m_nPreviousMoveState )
	{
		m_flStutterStep += 10;
	}
	m_nPreviousMoveState = m_pPlayer->m_iMoveState;
	m_flStutterStep = clamp( Approach( 0, m_flStutterStep, m_flLastUpdateIncrement * 40 ), 0, 100 );
	
	// recompute moveweight

	float flTargetMoveWeight = Lerp( m_flAnimDuckAmount, clamp(m_flSpeedAsPortionOfWalkTopSpeed, 0, 1), clamp(m_flSpeedAsPortionOfCrouchTopSpeed, 0, 1) );
	//flTargetMoveWeight *= RemapValClamped( m_flStutterStep, 90, 100, 1, 0 );

	if ( m_flMoveWeight <= flTargetMoveWeight )
	{
		m_flMoveWeight = flTargetMoveWeight;
	}
	else
	{
		m_flMoveWeight = Approach( flTargetMoveWeight, m_flMoveWeight, m_flLastUpdateIncrement * RemapValClamped( m_flStutterStep, 0.0f, 100.0f, 2, 20 ) );
	}

	Vector vecMoveYawDir;
	AngleVectors( QAngle(0, AngleNormalize( m_flFootYaw + m_flMoveYaw + 180 ), 0), &vecMoveYawDir );
	float flYawDeltaAbsDot = abs( DotProduct( m_vecVelocityNormalizedNonZero, vecMoveYawDir ) );
	m_flMoveWeight *= Bias( flYawDeltaAbsDot, 0.2 );

	float flMoveWeightWithAirSmooth = m_flMoveWeight * m_flInAirSmoothValue;

	// dampen move weight for landings
	flMoveWeightWithAirSmooth *= MAX( (1.0f - GetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) ), 0.55f );

	float flMoveCycleRate = 0;
	if ( m_flVelocityLengthXY > 0 )
	{
		flMoveCycleRate = m_pPlayer->GetSequenceCycleRate( m_pPlayer->GetModelPtr(), nWeaponMoveSeq );
		float flSequenceGroundSpeed = MAX( m_pPlayer->GetSequenceMoveDist( m_pPlayer->GetModelPtr(), nWeaponMoveSeq ) / ( 1.0f / flMoveCycleRate ), 0.001f );
		flMoveCycleRate *= m_flVelocityLengthXY / flSequenceGroundSpeed;

		flMoveCycleRate *= Lerp( m_flWalkToRunTransition, 1.0f, CSGO_ANIM_RUN_ANIM_PLAYBACK_MULTIPLIER );
	}

	float flLocalCycleIncrement = (flMoveCycleRate * m_flLastUpdateIncrement);
	m_flPrimaryCycle = ClampCycle( m_flPrimaryCycle + flLocalCycleIncrement );
	
	flMoveWeightWithAirSmooth = clamp( flMoveWeightWithAirSmooth, 0, 1 );
	UpdateAnimLayer( ANIMATION_LAYER_MOVEMENT_MOVE, nWeaponMoveSeq, flLocalCycleIncrement, flMoveWeightWithAirSmooth, m_flPrimaryCycle );


#ifndef CLIENT_DLL
	// blend in a strafe direction-change pose when the player changes strafe dir

	// get the user's left and right button pressed states
	bool moveRight = ( m_pPlayer->m_nButtons & ( IN_MOVERIGHT ) ) != 0;
	bool moveLeft = ( m_pPlayer->m_nButtons & ( IN_MOVELEFT ) ) != 0;
	bool moveForward = ( m_pPlayer->m_nButtons & ( IN_FORWARD ) ) != 0;
	bool moveBackward = ( m_pPlayer->m_nButtons & ( IN_BACK ) ) != 0;

	//Vector vForward, vRight;
	//AngleVectors( QAngle(0,m_flEyeYaw,0), &vForward, &vRight, NULL );
	//vForward *= 10;
	//vRight *= 10;
	//if ( moveRight )
	//	debugoverlay->AddTriangleOverlay( m_vecPositionCurrent + vRight * 2, m_vecPositionCurrent - vForward, m_vecPositionCurrent + vForward, 200, 0, 0, 255, true, 0 );
	//if ( moveLeft )
	//	debugoverlay->AddTriangleOverlay( m_vecPositionCurrent - vRight * 2, m_vecPositionCurrent + vForward, m_vecPositionCurrent - vForward, 200, 0, 0, 255, true, 0 );
	//if ( moveForward )
	//	debugoverlay->AddTriangleOverlay( m_vecPositionCurrent + vForward * 2, m_vecPositionCurrent + vRight, m_vecPositionCurrent - vRight, 200, 0, 0, 255, true, 0 );
	//if ( moveBackward )
	//	debugoverlay->AddTriangleOverlay( m_vecPositionCurrent - vForward * 2, m_vecPositionCurrent + vRight, m_vecPositionCurrent - vRight, 200, 0, 0, 255, true, 0 );

	Vector vecForward;
	Vector vecRight;
	AngleVectors( QAngle(0,m_flFootYaw,0), &vecForward, &vecRight, NULL );
	vecRight.NormalizeInPlace();
	float flVelToRightDot = DotProduct( m_vecVelocityNormalizedNonZero, vecRight );
	float flVelToForwardDot = DotProduct( m_vecVelocityNormalizedNonZero, vecForward );

	// We're interested in if the player's desired direction (indicated by their held buttons) is opposite their current velocity.
	// This indicates a strafing direction change in progress.

	bool bStrafeRight =		( m_flSpeedAsPortionOfWalkTopSpeed >= 0.73f && moveRight && !moveLeft && flVelToRightDot < -0.63f );
	bool bStrafeLeft =		( m_flSpeedAsPortionOfWalkTopSpeed >= 0.73f && moveLeft && !moveRight && flVelToRightDot > 0.63f );
	bool bStrafeForward =	( m_flSpeedAsPortionOfWalkTopSpeed >= 0.65f && moveForward && !moveBackward && flVelToForwardDot < -0.55f );
	bool bStrafeBackward =	( m_flSpeedAsPortionOfWalkTopSpeed >= 0.65f && moveBackward && !moveForward && flVelToForwardDot > 0.55f );
	
	m_pPlayer->m_bStrafing = ( bStrafeRight || bStrafeLeft || bStrafeForward || bStrafeBackward );
#endif

	if ( m_pPlayer->m_bStrafing )
	{
		if ( !m_bStrafeChanging )
		{
			m_flDurationStrafing = 0;

			#ifdef CLIENT_DLL
			if ( !m_bFirstRunSinceInit && !m_bStrafeChanging && m_bOnGround && m_pPlayer->m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].GetCurrentWeight() <= 0 )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_strafestart ) );
			}
			#endif
		}

		m_bStrafeChanging = true;

		m_flStrafeChangeWeight = Approach( 1, m_flStrafeChangeWeight, m_flLastUpdateIncrement * 20 );
		m_flStrafeChangeCycle = Approach( 0, m_flStrafeChangeCycle, m_flLastUpdateIncrement * 10 );

		m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_DIR ].SetValue( m_pPlayer, AngleNormalize( m_flMoveYaw ) );

		//float flCross = m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_CROSS ].GetValue( m_pPlayer );
		//flCross = clamp( Approach( m_bFeetCrossed ? 1 : 0, flCross, m_flLastUpdateIncrement * 10 ), 0, 1);
		//m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_CROSS ].SetValue( m_pPlayer, flCross );
		//m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_CROSS ].SetValue( m_pPlayer, 0 );

	}
	else if ( m_flStrafeChangeWeight > 0 )
	{
		m_flDurationStrafing += m_flLastUpdateIncrement;

		if ( m_flDurationStrafing > 0.08f )
			m_flStrafeChangeWeight = Approach( 0, m_flStrafeChangeWeight, m_flLastUpdateIncrement * 5 );

		m_nStrafeSequence = m_pPlayer->LookupSequence( "strafe" );
		float flRate = m_pPlayer->GetSequenceCycleRate( m_pPlayer->GetModelPtr(), m_nStrafeSequence );
		m_flStrafeChangeCycle = clamp( m_flStrafeChangeCycle + m_flLastUpdateIncrement * flRate, 0, 1 );
	}

	if ( m_flStrafeChangeWeight <= 0 )
	{
		m_bStrafeChanging = false;
	}



	// keep track of if the player is on the ground, and if the player has just touched or left the ground since the last check
	bool bPreviousGroundState = m_bOnGround;
	m_bOnGround = ( m_pPlayer->GetFlags() & FL_ONGROUND );

	m_bLandedOnGroundThisFrame = ( !m_bFirstRunSinceInit && bPreviousGroundState != m_bOnGround && m_bOnGround );
	m_bLeftTheGroundThisFrame = ( bPreviousGroundState != m_bOnGround && !m_bOnGround );

	float flDistanceFell = 0;
	if ( m_bLeftTheGroundThisFrame )
	{
		m_flLeftGroundHeight = m_vecPositionCurrent.z;
	}

	if ( m_bLandedOnGroundThisFrame )
	{
		flDistanceFell = abs( m_flLeftGroundHeight - m_vecPositionCurrent.z );
		float flDistanceFallNormalizedBiasRange = Bias( RemapValClamped( flDistanceFell, 12.0f, 72.0f, 0.0f, 1.0f ), 0.4f );

		//Msg( "Fell %f units, ratio is %f. ", flDistanceFell, flDistanceFallNormalizedBiasRange );
		//Msg( "Fell for %f secs, multiplier is %f\n", m_flDurationInAir, m_flLandAnimMultiplier );

		m_flLandAnimMultiplier = clamp( Bias( m_flDurationInAir, 0.3f ), 0.1f, 1.0f );
		m_flDuckAdditional = MAX( m_flLandAnimMultiplier, flDistanceFallNormalizedBiasRange );

		//Msg( "m_flDuckAdditional is %f\n", m_flDuckAdditional );
	}
	else
	{
		m_flDuckAdditional = Approach( 0, m_flDuckAdditional, m_flLastUpdateIncrement * 2 );
	}

	// the in-air smooth value is a fuzzier representation of if the player is on the ground or not.
	// It will approach 1 when the player is on the ground and 0 when in the air. Useful for blending jump animations.
	m_flInAirSmoothValue = Approach( m_bOnGround ? 1 : 0, m_flInAirSmoothValue, Lerp( m_flAnimDuckAmount, CSGO_ANIM_ONGROUND_FUZZY_APPROACH, CSGO_ANIM_ONGROUND_FUZZY_APPROACH_CROUCH ) * m_flLastUpdateIncrement );
	m_flInAirSmoothValue = clamp( m_flInAirSmoothValue, 0, 1 );



	m_flStrafeChangeWeight *= ( 1.0f - m_flAnimDuckAmount );
	m_flStrafeChangeWeight *= m_flInAirSmoothValue;
	m_flStrafeChangeWeight = clamp( m_flStrafeChangeWeight, 0, 1 );

	//if ( m_flStrafeChangeWeight > 0 && flMoveWeightWithAirSmooth <= 0.01f )
	//{
	//	if ( flStrafePose > 0.5f )
	//	{
	//		m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nWeaponMoveSeq, ANIMTAG_STARTCYCLE_W, 0, 1 );
	//	}
	//	else
	//	{
	//		m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nWeaponMoveSeq, ANIMTAG_STARTCYCLE_E, 0, 1 );
	//	}
	//}

	if ( m_nStrafeSequence != -1 )
		UpdateAnimLayer( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE, m_nStrafeSequence, 0, m_flStrafeChangeWeight, m_flStrafeChangeCycle );



	//ladders
	bool bPreviouslyOnLadder = m_bOnLadder;
	m_bOnLadder = !m_bOnGround && m_pPlayer->GetMoveType() == MOVETYPE_LADDER;
	bool bStartedLadderingThisFrame = ( !bPreviouslyOnLadder && m_bOnLadder );
	bool bStoppedLadderingThisFrame = ( bPreviouslyOnLadder && !m_bOnLadder );
	

	if ( bStartedLadderingThisFrame || bStoppedLadderingThisFrame )
	{
#ifdef CLIENT_DLL
		//m_footLeft.m_flLateralWeight = 0;
		//m_footRight.m_flLateralWeight = 0;
#endif
	}
	
	if ( m_flLadderWeight > 0 || m_bOnLadder )
	{

#ifndef CLIENT_DLL
		if ( bStartedLadderingThisFrame )
		{
			SetLayerSequence( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, SelectSequenceFromActMods( ACT_CSGO_CLIMB_LADDER ) );
		}
#endif

		if ( abs(m_flVelocityLengthZ) > 100 )
		{
			m_flLadderSpeed = Approach( 1, m_flLadderSpeed, m_flLastUpdateIncrement * 10.0f );
		}
		else
		{
			m_flLadderSpeed = Approach( 0, m_flLadderSpeed, m_flLastUpdateIncrement * 10.0f );
		}
		m_flLadderSpeed = clamp( m_flLadderSpeed, 0, 1 );

		if ( m_bOnLadder )
		{
			m_flLadderWeight = Approach( 1, m_flLadderWeight, m_flLastUpdateIncrement * 5.0f );
		}
		else
		{
			m_flLadderWeight = Approach( 0, m_flLadderWeight, m_flLastUpdateIncrement * 10.0f );
		}
		m_flLadderWeight = clamp( m_flLadderWeight, 0, 1 );

		Vector vecLadderNormal = m_pPlayer->GetLadderNormal();
		QAngle angLadder;
		VectorAngles( vecLadderNormal, angLadder );
		float flLadderYaw = AngleDiff( angLadder.y, m_flFootYaw );
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_YAW ].SetValue( m_pPlayer, flLadderYaw );

		//float flPlayerZ = m_pPlayer->GetAbsOrigin().z;
		//float flLadderClimbCycle = fmod( abs(flPlayerZ), 80.0f ) / 80.0f;
		//flLadderClimbCycle = ClampCycle( flPlayerZ < 0 ? (1.0f - flLadderClimbCycle) : flLadderClimbCycle );

		
		float flLadderClimbCycle = GetLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB );
		flLadderClimbCycle += (m_vecPositionCurrent.z - m_vecPositionLast.z) * Lerp( m_flLadderSpeed, 0.010f, 0.004f );

		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_SPEED ].SetValue( m_pPlayer, m_flLadderSpeed );

		if ( GetLayerActivity( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) == ACT_CSGO_CLIMB_LADDER )
		{
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, m_flLadderWeight );
		}
		
		SetLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, flLadderClimbCycle );

		// fade out jump if we're climbing
		if ( m_bOnLadder )
		{
			float flIdealJumpWeight = 1.0f - m_flLadderWeight;
			if ( GetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL ) > flIdealJumpWeight )
			{
				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, flIdealJumpWeight );
			}
		}
	}
	else
	{
		m_flLadderSpeed = 0;
	}


	//jumping
	if ( m_bOnGround )
	{
		if ( !m_bLanding && (m_bLandedOnGroundThisFrame || bStoppedLadderingThisFrame) )
		{
#ifndef CLIENT_DLL
			SetLayerSequence( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, SelectSequenceFromActMods( (m_flDurationInAir>1) ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT ) );
#endif
			SetLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 0 );
			m_bLanding = true;
		}
		m_flDurationInAir = 0;
		
		if ( m_bLanding && GetLayerActivity(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB) != ACT_CSGO_CLIMB_LADDER )
		{
#ifndef CLIENT_DLL
			m_bJumping = false;
#endif

			IncrementLayerCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, false );
			IncrementLayerCycle( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, false );

			m_tPoseParamMappings[ PLAYER_POSE_PARAM_JUMP_FALL ].SetValue( m_pPlayer, 0 );

			if ( IsLayerSequenceCompleted( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) )
			{
				m_bLanding = false;
				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 0 );
				//SetLayerRate( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 1.0f );
				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, 0 );
				m_flLandAnimMultiplier = 1.0f;
			}
			else
			{

				float flLandWeight = GetLayerIdealWeightFromSeqCycle( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB ) * m_flLandAnimMultiplier;

				// if we hit the ground crouched, reduce the land animation as a function of crouch, since the land animations move the head up a bit ( and this is undesirable )
				flLandWeight *= clamp( (1.0f - m_flAnimDuckAmount), 0.2f, 1.0f );

				SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, flLandWeight );
				
				// fade out jump because land is taking over
				float flCurrentJumpFallWeight = GetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
				if ( flCurrentJumpFallWeight > 0 )
				{
					flCurrentJumpFallWeight = Approach( 0, flCurrentJumpFallWeight, m_flLastUpdateIncrement * 10.0f );
					SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, flCurrentJumpFallWeight );
				}
				
			}
		}

#ifndef CLIENT_DLL
		if ( !m_bLanding && !m_bJumping && m_flLadderWeight <= 0 )
		{
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, 0 );
		}
#endif
	}
	else if ( !m_bOnLadder )
	{
		m_bLanding = false;

		// we're in the air
		if ( m_bLeftTheGroundThisFrame || bStoppedLadderingThisFrame )
		{ 
#ifndef CLIENT_DLL
			// If entered the air by jumping, then we already set the jump activity.
			// But if we're in the air because we strolled off a ledge or the floor collapsed or something,
			// we need to set the fall activity here.
			if ( !m_bJumping ) 
			{
				SetLayerSequence( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, SelectSequenceFromActMods( ACT_CSGO_FALL ) );
			}
#endif
			m_flDurationInAir = 0;
		}
		
		m_flDurationInAir += m_flLastUpdateIncrement;

//#ifndef CLIENT_DLL
//		Msg( "%f\n", m_flDurationInAir );
//#endif

		IncrementLayerCycle( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, false );

		// increase jump weight
		float flJumpWeight = GetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
		float flNextJumpWeight = GetLayerIdealWeightFromSeqCycle( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL );
		if ( flNextJumpWeight > flJumpWeight )
		{
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, flNextJumpWeight );
		}

		// bash any lingering land weight to zero
		float flLingeringLandWeight = GetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB );
		if ( flLingeringLandWeight > 0 )
		{
			flLingeringLandWeight *= smoothstep_bounds( 0.2f, 0.0f, m_flDurationInAir );
			SetLayerWeight( ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, flLingeringLandWeight );
		}

		// blend jump into fall. This is a no-op if we're playing a fall anim.
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_JUMP_FALL ].SetValue( m_pPlayer, clamp(smoothstep_bounds( 0.72f, 1.52f, m_flDurationInAir ),0,1) );

	}

}

Activity CCSGOPlayerAnimState::GetLayerActivity( animstate_layer_t nLayerIndex )
{
	MDLCACHE_CRITICAL_SECTION();
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return (Activity)GetSequenceActivity( m_pPlayer->GetModelPtr(), pLayer->GetSequence() );
	return ACT_INVALID;
}

bool CCSGOPlayerAnimState::IsLayerSequenceCompleted( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return ( (pLayer->GetCycle() + (m_flLastUpdateIncrement * pLayer->GetPlaybackRate())) >= 1 );
	return false;
}

void CCSGOPlayerAnimState::SetLayerRate( animstate_layer_t nLayerIndex, float flRate )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		pLayer->SetPlaybackRate( flRate );
}

void CCSGOPlayerAnimState::SetLayerCycle( animstate_layer_t nLayerIndex, float flCycle )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		pLayer->SetCycle( ClampCycle( flCycle ) );
}

void CCSGOPlayerAnimState::IncrementLayerCycleWeightRateGeneric( animstate_layer_t nLayerIndex )
{
	float flWeightPrevious = GetLayerWeight( nLayerIndex );
	IncrementLayerCycle( nLayerIndex, false );
	SetLayerWeight( nLayerIndex, GetLayerIdealWeightFromSeqCycle( nLayerIndex ) );
	SetLayerWeightRate( nLayerIndex, flWeightPrevious );
}

void CCSGOPlayerAnimState::IncrementLayerCycle( animstate_layer_t nLayerIndex, bool bAllowLoop )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );

	if ( !pLayer )
		return;

	if ( abs(pLayer->GetPlaybackRate()) <= 0 )
		return;

	float flCurrentCycle = pLayer->GetCycle();
	flCurrentCycle += m_flLastUpdateIncrement * pLayer->GetPlaybackRate();

	if ( !bAllowLoop && flCurrentCycle >= 1 )
	{
		flCurrentCycle = 0.999f;
	}

	pLayer->SetCycle( ClampCycle( flCurrentCycle ) );
}

void CCSGOPlayerAnimState::IncrementLayerWeight( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );

	if ( !pLayer )
		return;

	if ( abs(pLayer->GetWeightDeltaRate()) <= 0 )
		return;

	float flCurrentWeight = pLayer->GetWeight();
	flCurrentWeight += m_flLastUpdateIncrement * pLayer->GetWeightDeltaRate();
	flCurrentWeight = clamp( flCurrentWeight, 0, 1 );

	pLayer->SetWeight( flCurrentWeight );
}

void CCSGOPlayerAnimState::SetLayerSequence( animstate_layer_t nLayerIndex, int nSequence )
{
	Assert( nSequence > 1 );
	if ( nSequence > 1 )
	{
		MDLCACHE_CRITICAL_SECTION();

		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
		
		if ( !pLayer )
			return;

		pLayer->SetSequence( nSequence );
		float flPlaybackRate = m_pPlayer->GetLayerSequenceCycleRate( pLayer, nSequence );
		pLayer->SetPlaybackRate( flPlaybackRate );
		pLayer->SetCycle( 0 );
		pLayer->SetWeight( 0 );
		UpdateLayerOrderPreset( nLayerIndex, nSequence );
		#ifndef CLIENT_DLL
		pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 
		#endif
	}
}

float CCSGOPlayerAnimState::GetLayerIdealWeightFromSeqCycle( animstate_layer_t nLayerIndex )
{
	MDLCACHE_CRITICAL_SECTION();
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );

	if ( !pLayer )
		return 0;

	mstudioseqdesc_t &seqdesc = m_pPlayer->GetModelPtr()->pSeqdesc( pLayer->GetSequence() );

	float flCycle = pLayer->GetCycle();
	if ( flCycle >= 0.999f )
		flCycle = 1;
	float flEaseIn = seqdesc.fadeintime;
	float flEaseOut = seqdesc.fadeouttime;
	float flIdealWeight = 1;
	
	if ( flEaseIn > 0 && flCycle < flEaseIn )
	{
		flIdealWeight = smoothstep_bounds( 0, flEaseIn, flCycle );
	}
	else if ( flEaseOut < 1 && flCycle > flEaseOut )
	{
		flIdealWeight = smoothstep_bounds( 1.0f, flEaseOut, flCycle );
	}

	if ( flIdealWeight < 0.0015f )
		return 0;

	return (clamp( flIdealWeight, 0, 1));
}

float CCSGOPlayerAnimState::GetLayerCycle( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetCycle();
	return 0;
}

int CCSGOPlayerAnimState::GetLayerSequence( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetSequence();
	return 0;
}

float CCSGOPlayerAnimState::GetLayerRate( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetPlaybackRate();
	return 0;
}

float CCSGOPlayerAnimState::GetLayerWeight( animstate_layer_t nLayerIndex )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( pLayer )
		return pLayer->GetWeight();
	return 0;
}

void CCSGOPlayerAnimState::SetLayerWeight( animstate_layer_t nLayerIndex, float flWeight )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( !pLayer )
		return;
	pLayer->SetWeight( clamp( flWeight, 0, 1) );
	#ifndef CLIENT_DLL
	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 
	#endif
}

void CCSGOPlayerAnimState::SetLayerWeightRate( animstate_layer_t nLayerIndex, float flPrevious )
{
	if ( m_flLastUpdateIncrement == 0 )
		return;
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( !pLayer )
		return;
	float flNewRate = ( pLayer->GetWeight() - flPrevious ) / m_flLastUpdateIncrement;
	pLayer->SetWeightDeltaRate( flNewRate );
}

void CCSGOPlayerAnimState::UpdateAnimLayer( animstate_layer_t nLayerIndex, int nSequence, float flPlaybackRate, float flWeight, float flCycle )
{
	AssertOnce( flWeight >= 0 && flWeight <= 1 );
	AssertOnce( flCycle >= 0 && flCycle <= 1 );
	Assert( nSequence > 1 );
	
	Assert( nSequence > 1 );
	if ( nSequence > 1 )
	{
		MDLCACHE_CRITICAL_SECTION();	
	
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
		if ( !pLayer )
			return;
		pLayer->SetSequence( nSequence );
		pLayer->SetPlaybackRate( flPlaybackRate );
		pLayer->SetCycle( clamp( flCycle, 0, 1) );
		pLayer->SetWeight( clamp( flWeight, 0, 1) );
		UpdateLayerOrderPreset( nLayerIndex, nSequence );
		#ifndef CLIENT_DLL
		pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 
		#endif
	}
}

void CCSGOPlayerAnimState::ApplyLayerOrderPreset( animlayerpreset nNewPreset, bool bForce )
{
	if ( !bForce && m_pLayerOrderPreset == nNewPreset )
		return;

	m_pLayerOrderPreset = nNewPreset;

	for ( int i=0; i < ANIMATION_LAYER_COUNT; i++ )
	{
		CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( m_pLayerOrderPreset[i], USE_ANIMLAYER_RAW_INDEX );
		if ( pLayer )
		{
			pLayer->SetOrder( i );

			// purge dispatch info too
			pLayer->m_pDispatchedStudioHdr = NULL;
			pLayer->m_nDispatchedSrc = ACT_INVALID;
			pLayer->m_nDispatchedDst = ACT_INVALID;
		}
	}
}

void CCSGOPlayerAnimState::UpdateLayerOrderPreset( animstate_layer_t nLayerIndex, int nSequence )
{
	if ( !m_pPlayer || nLayerIndex != ANIMATION_LAYER_WEAPON_ACTION )
		return;
	
	MDLCACHE_CRITICAL_SECTION();

	if ( m_pPlayer->GetAnySequenceAnimTag( nSequence, ANIMTAG_WEAPON_POSTLAYER, 0 ) != 0 )
	{
		ApplyLayerOrderPreset( get_animlayerpreset( WeaponPost ) );
	}
	else
	{
		ApplyLayerOrderPreset( get_animlayerpreset( Default ) );
	}	
}

bool CCSGOPlayerAnimState::LayerSequenceHasActMod( animstate_layer_t nLayerIndex, const char* szActMod )
{
	CAnimationLayer *pLayer = m_pPlayer->GetAnimOverlay( nLayerIndex, USE_ANIMLAYER_RAW_INDEX );
	if ( !pLayer )
		return false;
	//CUtlSymbol sym = g_ActivityModifiersTable.Find( szActMod );
	//if ( sym.IsValid() )
	//{
		mstudioseqdesc_t &seqdesc = m_pPlayer->GetModelPtr()->pSeqdesc( pLayer->GetSequence() );
		for ( int i=0; i<seqdesc.numactivitymodifiers; i++ )
		{
			mstudioactivitymodifier_t *mod = seqdesc.pActivityModifier(i);

			if ( !V_strcmp( mod->pszName(), szActMod ) )
				return true;
		}
	//}
	return false;
}

#define CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX 0.8f
#define CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX_SCOPED 4.2f
void CCSGOPlayerAnimState::SetUpAimMatrix( void )
{
	MDLCACHE_CRITICAL_SECTION();
	
	if ( m_flAnimDuckAmount <= 0 || m_flAnimDuckAmount >= 1 ) // only transition aim pose when fully ducked or fully standing
	{
		bool bPlayerIsWalking = ( m_pPlayer && m_pPlayer->m_bIsWalking );
		bool bPlayerIsScoped = ( m_pPlayer && m_pPlayer->m_bIsScoped );

		float flTransitionSpeed = m_flLastUpdateIncrement * ( bPlayerIsScoped ? CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX_SCOPED : CSGO_ANIM_SPEED_TO_CHANGE_AIM_MATRIX );

		if ( bPlayerIsScoped ) // hacky: just tell all the transitions they've been invalid too long so all transitions clear as soon as the player starts scoping
		{
			m_tStandWalkAim.m_flDurationStateHasBeenInValid = m_tStandWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut;
			m_tStandRunAim.m_flDurationStateHasBeenInValid = m_tStandRunAim.m_flHowLongToWaitUntilTransitionCanBlendOut;
			m_tCrouchWalkAim.m_flDurationStateHasBeenInValid = m_tCrouchWalkAim.m_flHowLongToWaitUntilTransitionCanBlendOut;
		}

		m_tStandWalkAim.UpdateTransitionState( bPlayerIsWalking && !bPlayerIsScoped && m_flSpeedAsPortionOfWalkTopSpeed > 0.7f && m_flSpeedAsPortionOfRunTopSpeed < 0.7,
			m_flLastUpdateIncrement, flTransitionSpeed );

		m_tStandRunAim.UpdateTransitionState( !bPlayerIsScoped && m_flSpeedAsPortionOfRunTopSpeed >= 0.7,
			m_flLastUpdateIncrement, flTransitionSpeed );

		m_tCrouchWalkAim.UpdateTransitionState( !bPlayerIsScoped && m_flSpeedAsPortionOfCrouchTopSpeed >= 0.5,
			m_flLastUpdateIncrement, flTransitionSpeed );
	}

	// Set aims to zero weight if they're underneath aims with 100% weight, for animation perf optimization.
	// Also set aims to full weight if their overlapping aims aren't enough to cover them, because cross-fades don't sum to 100% weight.

	float flStandIdleWeight = 1;
	float flStandWalkWeight = m_tStandWalkAim.m_flBlendValue;
	float flStandRunWeight = m_tStandRunAim.m_flBlendValue;
	float flCrouchIdleWeight = 1;
	float flCrouchWalkWeight = m_tCrouchWalkAim.m_flBlendValue;

	if ( flStandWalkWeight >= 1 )
		flStandIdleWeight = 0;

	if ( flStandRunWeight >= 1 )
	{
		flStandIdleWeight = 0;
		flStandWalkWeight = 0;
	}

	if ( flCrouchWalkWeight >= 1 )
		flCrouchIdleWeight = 0;

	if ( m_flAnimDuckAmount >= 1 )
	{
		flStandIdleWeight = 0;
		flStandWalkWeight = 0;
		flStandRunWeight = 0;
	}
	else if ( m_flAnimDuckAmount <= 0 )
	{
		flCrouchIdleWeight = 0;
		flCrouchWalkWeight = 0;
	}

	float flOneMinusDuckAmount = 1.0f - m_flAnimDuckAmount;

	flCrouchIdleWeight *= m_flAnimDuckAmount;
	flCrouchWalkWeight *= m_flAnimDuckAmount;
	flStandWalkWeight *= flOneMinusDuckAmount;
	flStandRunWeight *= flOneMinusDuckAmount;

	// make sure idle is present underneath cross-fades
	if ( flCrouchIdleWeight < 1 && flCrouchWalkWeight < 1 && flStandWalkWeight < 1 && flStandRunWeight < 1 )
		flStandIdleWeight = 1;

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_IDLE ].SetValue( m_pPlayer, flStandIdleWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK ].SetValue( m_pPlayer, flStandWalkWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN ].SetValue( m_pPlayer, flStandRunWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_IDLE ].SetValue( m_pPlayer, flCrouchIdleWeight );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK ].SetValue( m_pPlayer, flCrouchWalkWeight );

	char szTransitionStandAimMatrix[MAX_ANIMSTATE_ANIMNAME_CHARS];
	V_sprintf_safe( szTransitionStandAimMatrix, "%s_aim", GetWeaponPrefix() );
	int nSeqStand = m_pPlayer->LookupSequence( szTransitionStandAimMatrix );

	{
		// use data-driven aim matrix limits

		CAnimationLayer *pAimLayer = m_pPlayer->GetAnimOverlay( ANIMATION_LAYER_AIMMATRIX, USE_ANIMLAYER_RAW_INDEX );
		if ( pAimLayer && m_pWeapon )
		{
			MDLCACHE_CRITICAL_SECTION();

			CBaseAnimating *pAimMatrixHolder = m_pPlayer;
			int nSeq = nSeqStand;

			CBaseWeaponWorldModel *pWeaponWorldModel = m_pWeapon->m_hWeaponWorldModel.Get();
			if ( pWeaponWorldModel && pAimLayer->m_nDispatchedDst != ACT_INVALID )
			{
				pAimMatrixHolder = pWeaponWorldModel;
				nSeq = pAimLayer->m_nDispatchedDst;
			}

			if ( nSeq > 0 )
			{
				float flYawIdleMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN );
				float flYawIdleMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX );
				float flYawWalkMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_WALK, flYawIdleMin );
				float flYawWalkMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_WALK, flYawIdleMax );
				float flYawRunMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_RUN, flYawWalkMin );
				float flYawRunMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_RUN, flYawWalkMax );
				float flYawCrouchIdleMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_CROUCHIDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MIN );
				float flYawCrouchIdleMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_CROUCHIDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_YAW_MAX );
				float flYawCrouchWalkMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMIN_CROUCHWALK, flYawCrouchIdleMin );
				float flYawCrouchWalkMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_YAWMAX_CROUCHWALK, flYawCrouchIdleMax );
				
				float flWalkAmt = m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK ].GetValue( m_pPlayer );
				float flRunAmt = m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN ].GetValue( m_pPlayer );
				float flCrouchWalkAmt = m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK ].GetValue( m_pPlayer );
			
				m_flAimYawMin = Lerp( m_flAnimDuckAmount, 
										Lerp( flRunAmt, Lerp( flWalkAmt, flYawIdleMin, flYawWalkMin ), flYawRunMin ),
										Lerp( flCrouchWalkAmt, flYawCrouchIdleMin, flYawCrouchWalkMin ) );
				m_flAimYawMax = Lerp( m_flAnimDuckAmount,
										Lerp( flRunAmt, Lerp( flWalkAmt, flYawIdleMax, flYawWalkMax ), flYawRunMax ),
										Lerp( flCrouchWalkAmt, flYawCrouchIdleMax, flYawCrouchWalkMax ) );

				float flPitchIdleMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN );
				float flPitchIdleMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_IDLE, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX );
				float flPitchWalkRunMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_WALKRUN, flPitchIdleMin );
				float flPitchWalkRunMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_WALKRUN, flPitchIdleMax );
				float flPitchCrouchMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_CROUCH, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN );
				float flPitchCrouchMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_CROUCH, CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX );
				float flPitchCrouchWalkMin = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMIN_CROUCHWALK, flPitchCrouchMin );
				float flPitchCrouchWalkMax = pAimMatrixHolder->GetAnySequenceAnimTag( nSeq, ANIMTAG_AIMLIMIT_PITCHMAX_CROUCHWALK, flPitchCrouchMax );

				m_flAimPitchMin = Lerp( m_flAnimDuckAmount, Lerp( flWalkAmt, flPitchIdleMin, flPitchWalkRunMin ), Lerp( flCrouchWalkAmt, flPitchCrouchMin, flPitchCrouchWalkMin ) );
				m_flAimPitchMax = Lerp( m_flAnimDuckAmount, Lerp( flWalkAmt, flPitchIdleMax, flPitchWalkRunMax ), Lerp( flCrouchWalkAmt, flPitchCrouchMax, flPitchCrouchWalkMax ) );
			}
		}
	}

	UpdateAnimLayer( ANIMATION_LAYER_AIMMATRIX, nSeqStand, 0, 1, 0 );
}

#define CSGO_ANIM_LOWER_CATCHUP_IDLE	100.0f
#define CSGO_ANIM_AIM_NARROW_WALK	0.8f
#define CSGO_ANIM_AIM_NARROW_RUN	0.5f
#define CSGO_ANIM_AIM_NARROW_CROUCHMOVING	0.5f
#define CSGO_ANIM_LOWER_CATCHUP_WITHIN	3.0f
#define CSGO_ANIM_LOWER_REALIGN_DELAY	1.1f
#define CSGO_ANIM_READJUST_THRESHOLD	120.0f
#define EIGHT_WAY_WIDTH 22.5f
void CCSGOPlayerAnimState::SetUpVelocity( void )
{
	MDLCACHE_CRITICAL_SECTION();

	// update the local velocity variables so we don't recalculate them unnecessarily

#ifndef  CLIENT_DLL
	Vector vecAbsVelocity = m_pPlayer->GetAbsVelocity();
#else
	Vector vecAbsVelocity = m_vecVelocity;

	if ( engine->IsHLTV() || engine->IsPlayingDemo() )
	{
		// Estimating velocity when playing demos is prone to fail, especially in POVs. Fall back to GetAbsVelocity.
		vecAbsVelocity = m_pPlayer->GetAbsVelocity();
	}
	else
	{
		m_pPlayer->EstimateAbsVelocity( vecAbsVelocity );	// Using this accessor if the client is starved of information, 
															// the player doesn't run on the spot. Note this is unreliable
															// and could fail to populate the value if prediction fails.
	}

	// prevent the client input velocity vector from exceeding a reasonable magnitude
	#define CSGO_ANIM_MAX_VEL_LIMIT 1.2f
	if ( vecAbsVelocity.LengthSqr() > Sqr( CS_PLAYER_SPEED_RUN * CSGO_ANIM_MAX_VEL_LIMIT ) )
		vecAbsVelocity = vecAbsVelocity.Normalized() * (CS_PLAYER_SPEED_RUN * CSGO_ANIM_MAX_VEL_LIMIT);

#endif

	// save vertical velocity component
	m_flVelocityLengthZ = vecAbsVelocity.z;

	// discard z component
	vecAbsVelocity.z = 0;
	
	// remember if the player is accelerating.
	m_bPlayerIsAccelerating = ( m_vecVelocityLast.LengthSqr() < vecAbsVelocity.LengthSqr() );

	// rapidly approach ideal velocity instead of instantly adopt it. This helps smooth out instant velocity changes, like
	// when the player runs headlong into a wall and their velocity instantly becomes zero.
	m_vecVelocity = Approach( vecAbsVelocity, m_vecVelocity, m_flLastUpdateIncrement * 2000 );
	m_vecVelocityNormalized = m_vecVelocity.Normalized();

	// save horizontal velocity length
	m_flVelocityLengthXY = MIN( m_vecVelocity.Length(), CS_PLAYER_SPEED_RUN );
	
	if ( m_flVelocityLengthXY > 0 )
	{
		m_vecVelocityNormalizedNonZero = m_vecVelocityNormalized;
	}

	//compute speed in various normalized forms
	float flMaxSpeedRun = m_pWeapon ? MAX( m_pWeapon->GetMaxSpeed(), 0.001f ) : CS_PLAYER_SPEED_RUN;
	Assert( flMaxSpeedRun > 0 );

	m_flSpeedAsPortionOfRunTopSpeed = clamp( m_flVelocityLengthXY / flMaxSpeedRun, 0, 1 );
	m_flSpeedAsPortionOfWalkTopSpeed = m_flVelocityLengthXY / (flMaxSpeedRun * CS_PLAYER_SPEED_WALK_MODIFIER);
	m_flSpeedAsPortionOfCrouchTopSpeed = m_flVelocityLengthXY / (flMaxSpeedRun * CS_PLAYER_SPEED_DUCK_MODIFIER);
	

	if ( m_flSpeedAsPortionOfWalkTopSpeed >= 1 )
	{
		m_flStaticApproachSpeed = m_flVelocityLengthXY;
	}
	else if ( m_flSpeedAsPortionOfWalkTopSpeed < 0.5f )
	{
		m_flStaticApproachSpeed = Approach( 80, m_flStaticApproachSpeed, m_flLastUpdateIncrement * 60 );
	}


	bool bStartedMovingThisFrame = false;
	bool bStoppedMovingThisFrame = false;

	if ( m_flVelocityLengthXY > 0 )
	{
		bStartedMovingThisFrame = ( m_flDurationMoving <= 0 );
		m_flDurationStill = 0;
		m_flDurationMoving += m_flLastUpdateIncrement;
	}
	else
	{
		bStoppedMovingThisFrame = ( m_flDurationStill <= 0 );
		m_flDurationMoving = 0;
		m_flDurationStill += m_flLastUpdateIncrement;
	}

#ifndef CLIENT_DLL
	if ( !m_bAdjustStarted && bStoppedMovingThisFrame && m_bOnGround && !m_bOnLadder && !m_bLanding && m_flStutterStep < 50 )
	{
		SetLayerSequence( ANIMATION_LAYER_ADJUST, SelectSequenceFromActMods( ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING ) );
		m_bAdjustStarted = true;
	}

	if ( GetLayerActivity( ANIMATION_LAYER_ADJUST ) == ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING ||
		 GetLayerActivity( ANIMATION_LAYER_ADJUST ) == ACT_CSGO_IDLE_TURN_BALANCEADJUST )
	{
		if ( m_bAdjustStarted && m_flSpeedAsPortionOfCrouchTopSpeed <= 0.25f )
		{
			IncrementLayerCycleWeightRateGeneric( ANIMATION_LAYER_ADJUST );
			m_bAdjustStarted = !( IsLayerSequenceCompleted( ANIMATION_LAYER_ADJUST ) );
		}
		else
		{
			m_bAdjustStarted = false;
			float flWeight = GetLayerWeight( ANIMATION_LAYER_ADJUST );
			SetLayerWeight( ANIMATION_LAYER_ADJUST, Approach( 0, flWeight, m_flLastUpdateIncrement * 5 ) );
			SetLayerWeightRate( ANIMATION_LAYER_ADJUST, flWeight );
		}
	}
#endif

	// if the player is looking far enough to either side, turn the feet to keep them within the extent of the aim matrix
	m_flFootYawLast = m_flFootYaw;
	m_flFootYaw = clamp( m_flFootYaw, -360, 360 );
	float flEyeFootDelta = AngleDiff(m_flEyeYaw, m_flFootYaw);

	// narrow the available aim matrix width as speed increases
	float flAimMatrixWidthRange = Lerp( clamp(m_flSpeedAsPortionOfWalkTopSpeed,0,1), 1.0f, Lerp( m_flWalkToRunTransition, CSGO_ANIM_AIM_NARROW_WALK, CSGO_ANIM_AIM_NARROW_RUN ) );

	if ( m_flAnimDuckAmount > 0 )
	{
		flAimMatrixWidthRange = Lerp( m_flAnimDuckAmount * clamp( m_flSpeedAsPortionOfCrouchTopSpeed, 0, 1 ), flAimMatrixWidthRange, CSGO_ANIM_AIM_NARROW_CROUCHMOVING );
	}
	
	float flTempYawMax = m_flAimYawMax * flAimMatrixWidthRange;
	float flTempYawMin = m_flAimYawMin * flAimMatrixWidthRange;

	if ( flEyeFootDelta > flTempYawMax )
	{
		m_flFootYaw = m_flEyeYaw - abs(flTempYawMax);
	}
	else if ( flEyeFootDelta < flTempYawMin )
	{
		m_flFootYaw = m_flEyeYaw + abs(flTempYawMin);
	}
	m_flFootYaw = AngleNormalize( m_flFootYaw );


	// pull the lower body direction towards the eye direction, but only when the player is moving
	if ( m_bOnGround )
	{
		if ( m_flVelocityLengthXY > 0.1f )
		{
			m_flFootYaw = ApproachAngle( m_flEyeYaw, m_flFootYaw, m_flLastUpdateIncrement * (30.0f + 20.0f * m_flWalkToRunTransition) );

			#ifndef CLIENT_DLL
			m_flLowerBodyRealignTimer = gpGlobals->curtime + ( CSGO_ANIM_LOWER_REALIGN_DELAY * 0.2f );
			m_pPlayer->m_flLowerBodyYawTarget.Set( m_flEyeYaw );
			#endif
		}
		else
		{
			m_flFootYaw = ApproachAngle( m_pPlayer->m_flLowerBodyYawTarget.Get(), m_flFootYaw, m_flLastUpdateIncrement * CSGO_ANIM_LOWER_CATCHUP_IDLE );

			#ifndef CLIENT_DLL
			if ( gpGlobals->curtime > m_flLowerBodyRealignTimer && abs( AngleDiff( m_flFootYaw, m_flEyeYaw ) ) > 35.0f )
			{
				m_flLowerBodyRealignTimer = gpGlobals->curtime + CSGO_ANIM_LOWER_REALIGN_DELAY;
				m_pPlayer->m_flLowerBodyYawTarget.Set( m_flEyeYaw );
			}
			#endif
		}
	}

#ifndef CLIENT_DLL
	if ( m_flVelocityLengthXY <= CS_PLAYER_SPEED_STOPPED && m_bOnGround && !m_bOnLadder && !m_bLanding && m_flLastUpdateIncrement > 0 && abs( AngleDiff( m_flFootYawLast, m_flFootYaw ) / m_flLastUpdateIncrement > CSGO_ANIM_READJUST_THRESHOLD ) )
	{
		SetLayerSequence( ANIMATION_LAYER_ADJUST, SelectSequenceFromActMods( ACT_CSGO_IDLE_TURN_BALANCEADJUST ) );
		m_bAdjustStarted = true;
	}
#endif

#ifdef CLIENT_DLL
	if ( GetLayerWeight( ANIMATION_LAYER_ADJUST ) > 0 )
	{
		IncrementLayerCycle( ANIMATION_LAYER_ADJUST, false );
		IncrementLayerWeight( ANIMATION_LAYER_ADJUST );
	}
#endif

	// the final model render yaw is aligned to the foot yaw

	if ( m_flVelocityLengthXY > 0 && m_bOnGround )
	{
		// convert horizontal velocity vec to angular yaw
		float flRawYawIdeal = (atan2(-m_vecVelocity[1], -m_vecVelocity[0]) * 180 / M_PI);
		if (flRawYawIdeal < 0)
			flRawYawIdeal += 360;

		m_flMoveYawIdeal = AngleNormalize( AngleDiff( flRawYawIdeal, m_flFootYaw ) );
	}

	// delta between current yaw and ideal velocity derived target (possibly negative!)
	m_flMoveYawCurrentToIdeal = AngleNormalize( AngleDiff( m_flMoveYawIdeal, m_flMoveYaw ) );


	if ( bStartedMovingThisFrame && m_flMoveWeight <= 0 )
	{
		m_flMoveYaw = m_flMoveYawIdeal;

		// select a special starting cycle that's set by the animator in content
		int nMoveSeq = GetLayerSequence( ANIMATION_LAYER_MOVEMENT_MOVE );
		if ( nMoveSeq != -1 )
		{
			mstudioseqdesc_t &seqdesc = m_pPlayer->GetModelPtr()->pSeqdesc( nMoveSeq );
			if ( seqdesc.numanimtags > 0 )
			{
				if ( abs( AngleDiff( m_flMoveYaw, 180 ) ) <= EIGHT_WAY_WIDTH ) //N
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_N, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 135 ) ) <= EIGHT_WAY_WIDTH ) //NE
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_NE, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 90 ) ) <= EIGHT_WAY_WIDTH ) //E
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_E, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 45 ) ) <= EIGHT_WAY_WIDTH ) //SE
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_SE, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, 0 ) ) <= EIGHT_WAY_WIDTH ) //S
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_S, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, -45 ) ) <= EIGHT_WAY_WIDTH ) //SW
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_SW, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, -90 ) ) <= EIGHT_WAY_WIDTH ) //W
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_W, 0, 1 );
				}
				else if ( abs( AngleDiff( m_flMoveYaw, -135 ) ) <= EIGHT_WAY_WIDTH ) //NW
				{
					m_flPrimaryCycle = m_pPlayer->GetFirstSequenceAnimTag( nMoveSeq, ANIMTAG_STARTCYCLE_NW, 0, 1 );
				}
			}
		}

		#ifdef CLIENT_DLL
		if ( m_flInAirSmoothValue >= 1 && !m_bFirstRunSinceInit && abs(m_flMoveYawCurrentToIdeal) > 45 && m_bOnGround && m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].GetCurrentWeight() <= 0 )
		{
			m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_movebegin ) );
		}
		#endif

	}
	else
	{
		if ( GetLayerWeight( ANIMATION_LAYER_MOVEMENT_STRAFECHANGE ) >= 1 )
		{
			m_flMoveYaw = m_flMoveYawIdeal;
		}
		else
		{
			#ifdef CLIENT_DLL
			if ( m_flInAirSmoothValue >= 1 && !m_bFirstRunSinceInit && abs(m_flMoveYawCurrentToIdeal) > 100 && m_bOnGround && m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].GetCurrentWeight() <= 0 )
			{
				m_pPlayer->m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetShouldCapture( bonesnapshot_get( cl_bonesnapshot_speed_movebegin ) );
			}
			#endif

			float flMoveWeight = Lerp( m_flAnimDuckAmount, clamp(m_flSpeedAsPortionOfWalkTopSpeed, 0, 1), clamp(m_flSpeedAsPortionOfCrouchTopSpeed, 0, 1) );
			float flRatio = Bias( flMoveWeight, 0.18f ) + 0.1f;
			
			m_flMoveYaw = AngleNormalize( m_flMoveYaw + ( m_flMoveYawCurrentToIdeal * flRatio ) );
		}
	}	

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_YAW ].SetValue( m_pPlayer, m_flMoveYaw );
	
	float flAimYaw = AngleDiff( m_flEyeYaw, m_flFootYaw );
	if ( flAimYaw >= 0 && m_flAimYawMax != 0 )
	{
		flAimYaw = (flAimYaw / m_flAimYawMax) * 60.0f;
	}
	else if ( m_flAimYawMin != 0 )
	{
		flAimYaw = (flAimYaw / m_flAimYawMin) * -60.0f;
	}

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_YAW ].SetValue( m_pPlayer, flAimYaw );

	// we need non-symmetrical arbitrary min/max bounds for vertical aim (pitch) too
	float flPitch = AngleDiff( m_flEyePitch, 0 );
	if ( flPitch > 0 )
	{
		flPitch = (flPitch / m_flAimPitchMax) * CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MAX;
	}
	else
	{
		flPitch = (flPitch / m_flAimPitchMin) * CSGO_ANIM_AIMMATRIX_DEFAULT_PITCH_MIN;
	}

	m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_PITCH ].SetValue( m_pPlayer, flPitch );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_SPEED ].SetValue( m_pPlayer, m_flSpeedAsPortionOfWalkTopSpeed );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_STAND ].SetValue( m_pPlayer, 1.0f - (m_flAnimDuckAmount*m_flInAirSmoothValue) );
}

#ifndef CLIENT_DLL
int CCSGOPlayerAnimState::SelectSequenceFromActMods( Activity iAct )
{
	MDLCACHE_CRITICAL_SECTION();
	Assert( m_pPlayer );
	int nSelectedActivity = m_pPlayer->SelectWeightedSequenceFromModifiers( iAct, m_ActivityModifiers.Base(), m_ActivityModifiers.Count() );

	Assert( nSelectedActivity != ACT_INVALID );
	return nSelectedActivity;
}

void CCSGOPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t animEvent, int nData )
{
	UpdateActivityModifiers();

	switch (animEvent) {
		case PLAYERANIMEVENT_THROW_GRENADE_UNDERHAND:
		{
			AddActivityModifier( "underhand" );
		}
		case PLAYERANIMEVENT_FIRE_GUN_PRIMARY:
		case PLAYERANIMEVENT_THROW_GRENADE:
		{
			if ( m_pWeapon && animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY && m_pWeapon->IsA( WEAPON_C4 ) )
			{
				SetLayerSequence( ANIMATION_LAYER_WHOLE_BODY, SelectSequenceFromActMods( ACT_CSGO_PLANT_BOMB ) );
				m_bPlantAnimStarted = true;
			}
			//else if ( m_pWeapon->GetWeaponType() == WEAPONTYPE_KNIFE )
			//{
			//	// HACK: knives keep previous firing weight, if any
			//	float flSavedWeight = GetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION );
			//	SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_PRIMARY ) );
			//	SetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION, flSavedWeight );
			//}
			else
			{
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_PRIMARY ) );
			}			
			break;
		}
		case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT:
		{
			//if ( m_pWeapon->GetWeaponType() == WEAPONTYPE_KNIFE )
			//{
			//	// HACK: knives keep previous firing weight, if any
			//	float flSavedWeight = GetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION );
			//	SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_PRIMARY_OPT_1 ) );
			//	SetLayerWeight( ANIMATION_LAYER_WEAPON_ACTION, flSavedWeight );
			//}
			//else
			{
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_PRIMARY_OPT_1 ) );
			}
			break;
		}
		case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1:
		case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_PRIMARY_OPT_2 ) );
			break;
		}
		case PLAYERANIMEVENT_FIRE_GUN_SECONDARY:
		{
			if ( m_pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE )
			{
				// hack: sniper rifles use primary fire anim when 'alt' firing, meaning scoped.
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_PRIMARY ) );
			}
			else
			{
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_SECONDARY ) );
			}
			break;
		}
		case PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_FIRE_SECONDARY_OPT_1 ) );
			break;
		}
		case PLAYERANIMEVENT_GRENADE_PULL_PIN:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_OPERATE ) );
			break;
		}
		case PLAYERANIMEVENT_SILENCER_ATTACH:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_SILENCER_ATTACH ) );
			break;
		}
		case PLAYERANIMEVENT_SILENCER_DETACH:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_SILENCER_DETACH ) );
			break;
		}
		case PLAYERANIMEVENT_RELOAD:
		{
			if ( m_pWeapon && m_pWeapon->GetWeaponType() == WEAPONTYPE_SHOTGUN && m_pWeapon->GetCSWeaponID() != WEAPON_MAG7 )
				break;

			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_RELOAD ) );
			break;
		}
		case PLAYERANIMEVENT_RELOAD_START:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_RELOAD_START ) );
			break;
		}
		case PLAYERANIMEVENT_RELOAD_LOOP:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_RELOAD_LOOP ) );
			break;
		}
		case PLAYERANIMEVENT_RELOAD_END:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_RELOAD_END ) );
			break;
		}
		case PLAYERANIMEVENT_CATCH_WEAPON:
		{
			SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_CATCH ) );
			break;
		}
		case PLAYERANIMEVENT_CLEAR_FIRING:
		{
			m_bPlantAnimStarted = false;
			break;
		}
		//case PLAYERANIMEVENT_TAUNT:
		//{
		//	//not implemented yet
		//	break;
		//}
		case PLAYERANIMEVENT_DEPLOY:
		{
			if ( m_pWeapon && GetLayerActivity( ANIMATION_LAYER_WEAPON_ACTION ) == ACT_CSGO_DEPLOY &&
				 GetLayerCycle( ANIMATION_LAYER_WEAPON_ACTION ) < CSGO_ANIM_DEPLOY_RATELIMIT )
			{
				// we're already deploying
				m_bDeployRateLimiting = true;
			}
			else
			{
				SetLayerSequence( ANIMATION_LAYER_WEAPON_ACTION, SelectSequenceFromActMods( ACT_CSGO_DEPLOY ) );
			}
			break;
		}
		case PLAYERANIMEVENT_JUMP:
		{
			// note: this event means a jump is definitely happening, not just that a jump is desired
			m_bJumping = true;
			SetLayerSequence( ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, SelectSequenceFromActMods( ACT_CSGO_JUMP ) );
			break;
		}
		default:
		{
			break;
		}

	}

}

void CCSGOPlayerAnimState::AddActivityModifier( const char *szName )
{
	if ( szName == NULL )
	{
		Assert(0);
		return;
	}
	
	char szLookup[32];
	V_strcpy_safe( szLookup, szName );

	CUtlSymbol sym = g_ActivityModifiersTable.Find( szLookup );
	if ( !sym.IsValid() )
	{
		sym = g_ActivityModifiersTable.AddString( szLookup );
	}
	m_ActivityModifiers.AddToTail( sym );
}

void CCSGOPlayerAnimState::UpdateActivityModifiers( void )
{
	m_ActivityModifiers.Purge();

	AddActivityModifier( GetWeaponPrefix() );
	
	if ( m_flSpeedAsPortionOfWalkTopSpeed > 0.25f )
	{
		AddActivityModifier( "moving" );
	}

	if ( m_flAnimDuckAmount > 0.55f )
	{
		AddActivityModifier( "crouch" );
	}
}

void CCSGOPlayerAnimState::SelectDeathPose( const CTakeDamageInfo &info, int hitgroup, Activity& activity, float& yaw )
{
	activity = ACT_INVALID;
	yaw = 0;

	if ( !m_pPlayer )
		return;


	if ( hitgroup == HITGROUP_HEAD )
	{
		activity = ( m_flAnimDuckAmount > 0.5f ) ? ACT_DIE_CROUCH_HEADSHOT : ACT_DIE_STAND_HEADSHOT;
	}
	else
	{
		activity = ( m_flAnimDuckAmount > 0.5f ) ? ACT_DIE_CROUCH : ACT_DIE_STAND;
	}


	Vector vecDamageDir = info.GetDamageForce();
	VectorNormalize( vecDamageDir );

	Vector vecDamagePlusPlayerVel = vecDamageDir + ( m_vecVelocity.Normalized() * MIN(m_vecVelocity.Length() / CS_PLAYER_SPEED_RUN, 1) );
	VectorNormalize( vecDamagePlusPlayerVel );


	QAngle angDamageAnglePlusCurrentMoveVelocity;
	VectorAngles( vecDamageDir, Vector(0,0,1), angDamageAnglePlusCurrentMoveVelocity );
	
	float flPlayerRelativeDamageAngle = AngleDiff( angDamageAnglePlusCurrentMoveVelocity[YAW], m_flFootYaw );
	m_tPoseParamMappings[ PLAYER_POSE_PARAM_DEATH_YAW ].SetValue( m_pPlayer, clamp( flPlayerRelativeDamageAngle, -180, 180 ) );

	yaw = flPlayerRelativeDamageAngle;

	////float flBaseSize = 10;
	////float flHeight = 80;
	////Vector vBasePos = m_pPlayer->GetAbsOrigin() + Vector( 0, 0, 3 );
	////Vector vForward, vRight, vUp;
	////AngleVectors( angDamageAnglePlusCurrentMoveVelocity, &vForward, &vRight, &vUp );
	////debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 255, false, 10 );
}
#endif

const char* CCSGOPlayerAnimState::GetWeaponPrefix( void )
{
	int nWeaponType = 0; // knife

	m_pWeapon = m_pPlayer->GetActiveCSWeapon();
	if ( m_pWeapon )
	{
		nWeaponType = (int)m_pWeapon->GetWeaponType();

		int nWeaponID = m_pWeapon->GetCSWeaponID();
		if ( nWeaponID == WEAPON_MAG7 )
		{
			nWeaponType = WEAPONTYPE_RIFLE;
		}
		else if ( nWeaponID == WEAPON_TASER )
		{
			nWeaponType = WEAPONTYPE_PISTOL;
		}

		if ( nWeaponType == WEAPONTYPE_STACKABLEITEM )
		{
			nWeaponType = WEAPONTYPE_GRENADE; // redirect healthshot, adrenaline, etc to the grenade archetype
		}
	}

	return g_szWeaponPrefixLookupTable[clamp( nWeaponType, 0, 9 )];
}

void CCSGOPlayerAnimState::WriteAnimstateDebugBuffer( char *pDest )
{
}

bool CCSGOPlayerAnimState::CacheSequences( void )
{
	if ( !m_pPlayer )
		return false;

	if ( m_cachedModelIndex != m_pPlayer->GetModelIndex() )
	{
		// read animset version from keyvalues
		m_nAnimstateModelVersion = 0;

		CUtlBuffer buf( 1024, 0, CUtlBuffer::TEXT_BUFFER );
		buf.PutString( "keyvalues {\n" ); // trick the kv loader into thinking this is one big block
		if ( modelinfo->GetModelKeyValue( m_pPlayer->GetModel(), buf ) )
		{
			buf.PutString( "\n}\0" ); // end trickery
			KeyValues *modelKeyValues = new KeyValues("");
			KeyValues::AutoDelete autodelete_key( modelKeyValues );
			if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( m_pPlayer->GetModel() ), buf ) )
			{
				FOR_EACH_SUBKEY( modelKeyValues, pSubKey )
				{
					//KeyValuesDumpAsDevMsg( pSubKey );
					if ( pSubKey->FindKey( "animset_version" ) )
					{
						m_nAnimstateModelVersion = pSubKey->GetInt( "animset_version", CURRENT_ANIMSTATE_VERSION );
						break;
					}
				}
			}
		}

		AssertMsgOnce( m_nAnimstateModelVersion == CURRENT_ANIMSTATE_VERSION, "Animset version is out of date!" );

		// cache pose param indices
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LEAN_YAW				].Init( m_pPlayer, "lean_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_SPEED					].Init( m_pPlayer, "speed"					);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_SPEED			].Init( m_pPlayer, "ladder_speed"			);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_LADDER_YAW				].Init( m_pPlayer, "ladder_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_YAW				].Init( m_pPlayer, "move_yaw"				);

		if ( m_nAnimstateModelVersion < 2 )
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_RUN					].Init( m_pPlayer, "run"					);

		m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_YAW				].Init( m_pPlayer, "body_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_BODY_PITCH				].Init( m_pPlayer, "body_pitch"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_DEATH_YAW				].Init( m_pPlayer, "death_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_STAND					].Init( m_pPlayer, "stand"					);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_JUMP_FALL				].Init( m_pPlayer, "jump_fall"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_IDLE	].Init( m_pPlayer, "aim_blend_stand_idle"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_IDLE	].Init( m_pPlayer, "aim_blend_crouch_idle"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_STRAFE_DIR				].Init( m_pPlayer, "strafe_yaw"				);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_WALK	].Init( m_pPlayer, "aim_blend_stand_walk"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_STAND_RUN		].Init( m_pPlayer, "aim_blend_stand_run"	);
		m_tPoseParamMappings[ PLAYER_POSE_PARAM_AIM_BLEND_CROUCH_WALK	].Init( m_pPlayer, "aim_blend_crouch_walk"	);

		if ( m_nAnimstateModelVersion > 0 )
		{
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_WALK			].Init( m_pPlayer, "move_blend_walk"	);
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_RUN			].Init( m_pPlayer, "move_blend_run"		);
			m_tPoseParamMappings[ PLAYER_POSE_PARAM_MOVE_BLEND_CROUCH_WALK	].Init( m_pPlayer, "move_blend_crouch"	);
		}

		m_cachedModelIndex = m_pPlayer->GetModelIndex();

		//// precache the sequences we'll be using for movement
		//if ( m_cachedModelIndex > 0 && m_pPlayer->GetModelPtr() )
		//{
		//	bool bSuccess = true;
		//	bSuccess = bSuccess && PopulateSequenceCache( "move",					ANIMCACHE_MOVE );
		//	bSuccess = bSuccess && PopulateSequenceCache( "additive_posebreaker",	ANIMCACHE_ALIVELOOP );
		//	bSuccess = bSuccess && PopulateSequenceCache( "jump",					ANIMCACHE_JUMP );
		//	bSuccess = bSuccess && PopulateSequenceCache( "jump_moving",			ANIMCACHE_JUMP_MOVING );
		//	bSuccess = bSuccess && PopulateSequenceCache( "fall",					ANIMCACHE_FALL );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_light",				ANIMCACHE_LANDLIGHT );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_heavy",				ANIMCACHE_LANDHEAVY );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_light_crouched",	ANIMCACHE_LANDLIGHT_CROUCHED );
		//	bSuccess = bSuccess && PopulateSequenceCache( "land_heavy_crouched",	ANIMCACHE_LANDHEAVY_CROUCHED );
		//	bSuccess = bSuccess && PopulateSequenceCache( "ladder_climb",			ANIMCACHE_LADDERCLIMB );
		//	bSuccess = bSuccess && PopulateSequenceCache( "lean",					ANIMCACHE_LEAN );
		//	return bSuccess;
		//}
	}
	return m_cachedModelIndex > 0;
}

//----------------------------------------------------------------------------------
int animstate_pose_param_cache_t::GetIndex( void )
{
	Assert( m_bInitialized );
	return m_nIndex;
}

float animstate_pose_param_cache_t::GetValue( CCSPlayer* pPlayer )
{
	if ( !m_bInitialized )
	{
		Init( pPlayer, m_szName );
	}
	if ( m_bInitialized && pPlayer )
	{
		return pPlayer->GetPoseParameter( m_nIndex );
	}
	return 0;
}

void animstate_pose_param_cache_t::SetValue( CCSPlayer* pPlayer, float flValue )
{
	if ( !m_bInitialized )
	{
		Init( pPlayer, m_szName );
	}
	if ( m_bInitialized && pPlayer )
	{
		MDLCACHE_CRITICAL_SECTION();
		pPlayer->SetPoseParameter( m_nIndex, flValue );
	}
}

bool animstate_pose_param_cache_t::Init( CCSPlayer* pPlayer, const char* szPoseParamName )
{
	MDLCACHE_CRITICAL_SECTION();
	m_szName = szPoseParamName;
	m_nIndex = pPlayer->LookupPoseParameter( szPoseParamName );
	if ( m_nIndex != -1 )
	{
		m_bInitialized = true;
	}
	return m_bInitialized;
}
//----------------------------------------------------------------------------------
