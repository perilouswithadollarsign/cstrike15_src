//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "portal_playeranimstate.h"
#include "base_playeranimstate.h"
#include "movevars_shared.h"

#ifdef CLIENT_DLL
#include "C_Portal_Player.h"
#include "C_Weapon_Portalgun.h"
#include "c_te_effect_dispatch.h"
#include "particle_parse.h"
#else
#include "Portal_Player.h"
#include "Weapon_Portalgun.h"
#endif

#define PORTAL_RUN_SPEED			320.0f
#define PORTAL_CROUCHWALK_SPEED		110.0f

ConVar anim_forcedamaged( "anim_forcedamaged", "0", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Force the player to use the secondary damaged animations." );
ConVar anim_min_collision_speed_threshold("anim_min_collision_speed_threshold", "195.f", FCVAR_CHEAT | FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
// Output : CMultiPlayerAnimState*
//-----------------------------------------------------------------------------
CPortalPlayerAnimState* CreatePortalPlayerAnimState( CPortal_Player *pPlayer )
{
	// Setup the movement data.
	MultiPlayerMovementData_t movementData;
	movementData.m_flBodyYawRate = 720.0f;
	movementData.m_flRunSpeed = PORTAL_RUN_SPEED;
	movementData.m_flWalkSpeed = -1;
	movementData.m_flSprintSpeed = -1.0f;

	// Create animation state for this player.
	CPortalPlayerAnimState *pRet = new CPortalPlayerAnimState( pPlayer, movementData );

	// Specific Portal player initialization.
	pRet->InitPortal( pPlayer );

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CPortalPlayerAnimState::CPortalPlayerAnimState()
{
	m_pPortalPlayer = NULL;

	// Don't initialize Portal specific variables here. Init them in InitPortal()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//			&movementData - 
//-----------------------------------------------------------------------------
CPortalPlayerAnimState::CPortalPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData )
: CMultiPlayerAnimState( pPlayer, movementData )
{
	m_pPortalPlayer = NULL;

	// Don't initialize Portal specific variables here. Init them in InitPortal()
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CPortalPlayerAnimState::~CPortalPlayerAnimState()
{
}

//-----------------------------------------------------------------------------
// Purpose: Initialize Portal specific animation state.
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::InitPortal( CPortal_Player *pPlayer )
{
	m_pPortalPlayer = pPlayer;
	m_bInAirWalk = false;
	m_flHoldDeployedPoseUntilTime = 0.0f;
	m_bLanding = false;
	m_bWasInTractorBeam = false;
	m_bFirstTractorBeamFrame = false;
	m_bBridgeRemovedFromUnder = false;
	m_bDying = false;

	m_nDamageStage = DAMAGE_STAGE_NONE;

	m_fNextBouncePredictTime = 0.0f;
	m_fPrevBouncePredict = 4.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::ClearAnimationState( void )
{
	m_bInAirWalk = false;
	m_vLastVelocity = vec3_origin;
	m_bLanding = false;
	m_bWasInTractorBeam = false;
	m_bFirstTractorBeamFrame = false;
	m_bBridgeRemovedFromUnder = false;
	m_bDying = false;

	m_nDamageStage = DAMAGE_STAGE_NONE;

	BaseClass::ClearAnimationState();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : actDesired - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CPortalPlayerAnimState::TranslateActivity( Activity actDesired )
{
	Activity translateActivity = BaseClass::TranslateActivity( actDesired );

	// if injured
	if ( m_nDamageStage == DAMAGE_STAGE_FINAL )
	{
		switch ( translateActivity )
		{
		case ACT_MP_STAND_IDLE:
			{
				translateActivity = ACT_MP_STAND_SECONDARY;
				break;
			}
		case ACT_MP_RUN:
			{
				translateActivity = ACT_MP_RUN_SECONDARY;
				break;
			}
		}
	}

	if ( GetPortalPlayer()->GetActiveWeapon() )
	{
		translateActivity = GetPortalPlayer()->GetActiveWeapon()->ActivityOverride( translateActivity, false );
	}

	return translateActivity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : event - 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	Activity iWeaponActivity = ACT_INVALID;

#if defined CLIENT_DLL
	RANDOM_CEG_TEST_SECRET();
#endif

	switch( event )
	{
	case PLAYERANIMEVENT_ATTACK_PRIMARY:
	case PLAYERANIMEVENT_ATTACK_SECONDARY:
		{
			CPortal_Player *pPlayer = GetPortalPlayer();
			if ( !pPlayer )
				return;

			CWeaponPortalBase *pWpn = pPlayer->GetActivePortalWeapon();

			if ( pWpn )
			{
				// Weapon primary fire.
				if ( GetBasePlayer()->GetFlags() & FL_DUCKING )
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_CROUCH_PRIMARYFIRE );
				}
				else
				{
					RestartGesture( GESTURE_SLOT_ATTACK_AND_RELOAD, ACT_MP_ATTACK_STAND_PRIMARYFIRE );
				}

				iWeaponActivity = ACT_VM_PRIMARYATTACK;
			}
			else	// unarmed player
			{
				
			}
	
			break;
		}
	case PLAYERANIMEVENT_JUMP:
		{
			m_bInAirWalk = false;
			m_bLanding = false;
			BaseClass::DoAnimationEvent( event, nData );
			break;
		}
	case PLAYERANIMEVENT_DIE:
		{
			m_bDying = true;
			break;
		}
	case PLAYERANIMEVENT_FLINCH_CHEST:
	case PLAYERANIMEVENT_FLINCH_HEAD:
	case PLAYERANIMEVENT_FLINCH_LEFTARM:
	case PLAYERANIMEVENT_FLINCH_RIGHTARM:
	case PLAYERANIMEVENT_FLINCH_LEFTLEG:
	case PLAYERANIMEVENT_FLINCH_RIGHTLEG:
		{
			IncreaseDamageStage();
			BaseClass::DoAnimationEvent( event, nData );
			break;
		}
	default:
		{
			BaseClass::DoAnimationEvent( event, nData );
			break;
		}
	}

#ifdef CLIENT_DLL
	// Make the weapon play the animation as well
	if ( iWeaponActivity != ACT_INVALID )
	{
		CBaseCombatWeapon *pWeapon = GetPortalPlayer()->GetActiveWeapon();
		if ( pWeapon )
		{
			pWeapon->SendWeaponAnim( iWeaponActivity );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalPlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	// Profile the animation update.
	VPROF( "CPortalPlayerAnimState::Update" );

	// Get the player
	CPortal_Player *pPlayer = GetPortalPlayer();
	if ( pPlayer == NULL )
		return;

	// Get the studio header for the player.
	CStudioHdr *pStudioHdr = pPlayer->GetModelPtr();
	if ( !pStudioHdr )
		return;

	// Check to see if we should be updating the animation state - dead, ragdolled?
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}

	// Store the eye angles.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	// Compute the player sequences.
	ComputeSequences( pStudioHdr );

	if ( SetupPoseParameters( pStudioHdr ) )
	{
		// Pose parameter - what direction are the player's legs running in.
		ComputePoseParam_MoveYaw( pStudioHdr );

		// Pose parameter - Torso aiming (up/down).
		ComputePoseParam_AimPitch( pStudioHdr );

		// Pose parameter - Torso aiming (rotation).
		ComputePoseParam_AimYaw( pStudioHdr );
	}

	// Store this for collision results
	GetOuterAbsVelocity( m_vLastVelocity );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEG_NOINLINE void CPortalPlayerAnimState::Teleport( const Vector *pNewOrigin, const QAngle *pNewAngles, CPortal_Player* pPlayer )
{
	QAngle absangles = pPlayer->GetAbsAngles();
	m_angRender = absangles;
	m_angRender.x = m_angRender.z = 0.0f;
	if ( pPlayer )
	{
#if defined GAME_DLL
		CEG_PROTECT_MEMBER_FUNCTION( CPortalPlayerAnimState_Teleport );
#endif
		// Snap the yaw pose parameter lerping variables to face new angles.
		m_flCurrentFeetYaw = m_flGoalFeetYaw = m_flEyeYaw = pPlayer->EyeAngles()[YAW];
	}
}

void CPortalPlayerAnimState::TransformYAWs( const matrix3x4_t &matTransform )
{
	QAngle qOldAngles = vec3_angle;
	QAngle qAngles;

	qOldAngles[YAW] = m_flEyeYaw;
	qAngles = TransformAnglesToWorldSpace( qOldAngles, matTransform );
	m_flEyeYaw = qAngles[YAW];

	qOldAngles[YAW] = m_flGoalFeetYaw;
	qAngles = TransformAnglesToWorldSpace( qOldAngles, matTransform );
	m_flGoalFeetYaw = qAngles[YAW];

	qOldAngles[YAW] = m_flCurrentFeetYaw;
	qAngles = TransformAnglesToWorldSpace( qOldAngles, matTransform );
	m_flCurrentFeetYaw = qAngles[YAW];
}

bool CPortalPlayerAnimState::ShouldLongFall( void ) const
{
	CPortal_Player *pPortalPlayer = GetPortalPlayer();

	return ( m_bWasInTractorBeam || 
			 m_bBridgeRemovedFromUnder || 
			 ( !pPortalPlayer->GetTractorBeam() && 
			   pPortalPlayer->GetAirTime() > 2.0f && 
			   pPortalPlayer->GetAbsVelocity().AsVector2D().Length() < 450.0f ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPortalPlayerAnimState::HandleMoving( Activity &idealActivity )
{
	float flSpeed = GetOuterXYSpeed();
	CPortal_Player *pPortalPlayer = GetPortalPlayer();

	// If we're off the ground and not moving, do an airwalk
	bool bOnGround = ( pPortalPlayer->GetFlags() & FL_ONGROUND );
	if ( bOnGround == false )
	{
		if ( m_bWasInTractorBeam || m_bBridgeRemovedFromUnder )
		{
			idealActivity = ACT_MP_LONG_FALL;
		}
		else
		{
			idealActivity = ACT_MP_AIRWALK;
		}

		m_bInAirWalk = true;
	}
	else
	{
		CEG_GCV_PRE();
		static const int CEG_SPEED_POWER = CEG_GET_CONSTANT_VALUE( PaintSpeedPower );
		CEG_GCV_POST();
		bool bHasSpeedPower = pPortalPlayer->GetPaintPower( CEG_SPEED_POWER ).m_State == ACTIVE_PAINT_POWER;

#ifdef CLIENT_DLL
		if ( engine->HasPaintmap() && !bHasSpeedPower && !pPortalPlayer->IsLocalPlayer() )
		{
			// FIXME: Is this doing extra work in splitscreen?
			// Non-local players don't update paint powers on the client because this has to happen in gamemovement!
			// Quickly figure out if speed paint should be active
			CPortal_Player::PaintPowerInfoVector activePowers;
			pPortalPlayer->ChooseActivePaintPowers( activePowers );

			PaintPowerConstRange activeRange = GetConstRange( activePowers );
			for( PaintPowerConstIter i = activeRange.first; i != activeRange.second; ++i )
			{
				const PaintPowerInfo_t &newPower = *i;
				if ( newPower.m_PaintPowerType == CEG_SPEED_POWER )
				{
					bHasSpeedPower = true;
				}
			}

			// Clear the surface information
			// NOTE: Calling this after Activating/Using/Deactivating paint powers makes sticky boxes not very sticky
			//		 and that's why I moved it back over here. -Brett
			pPortalPlayer->ClearSurfacePaintPowerInfo();
		}
#endif

		if ( flSpeed > MOVING_MINIMUM_SPEED && bHasSpeedPower )
		{
			idealActivity = ACT_MP_RUN_SPEEDPAINT;
		}
		else if ( flSpeed > MOVING_MINIMUM_SPEED )
		{
			m_flHoldDeployedPoseUntilTime = 0.0;
			idealActivity = ACT_MP_RUN;
		}
		else if ( m_flHoldDeployedPoseUntilTime > gpGlobals->curtime )
		{
			// Unless we move, hold the deployed pose for a number of seconds after being deployed
			idealActivity = ACT_MP_DEPLOYED_IDLE;
		}
		else
		{
			return BaseClass::HandleMoving( idealActivity );
		}
	}

	if ( idealActivity == ACT_MP_RUN && anim_forcedamaged.GetBool() )
	{
		idealActivity = ACT_MP_RUN_SECONDARY;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
Activity CPortalPlayerAnimState::CalcMainActivity()
{
	Activity idealActivity = BaseClass::CalcMainActivity();
	if ( HandleBouncing( idealActivity ) ||
		 HandleLanding() ||
		 HandleTractorBeam( idealActivity ) ||
		 HandleInAir( idealActivity ) )
	{
		if ( idealActivity == ACT_MP_DOUBLEJUMP && m_eCurrentMainSequenceActivity != ACT_MP_DOUBLEJUMP )
		{
			m_pPlayer->SetCycle( 0 );
		}
	}

	if (idealActivity == ACT_MP_STAND_IDLE && anim_forcedamaged.GetBool())
	{
		idealActivity = ACT_MP_STAND_SECONDARY;
	}
	return idealActivity;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *idealActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPortalPlayerAnimState::HandleDucking( Activity &idealActivity )
{
	if ( ( GetBasePlayer()->GetFlags() & FL_DUCKING ) || GetBasePlayer()->m_Local.m_bDucking || GetBasePlayer()->m_Local.m_bDucked )
	{
		if ( GetOuterXYSpeed() < MOVING_MINIMUM_SPEED )
		{
			idealActivity = ACT_MP_CROUCH_IDLE;		
		}
		else
		{
			idealActivity = ACT_MP_CROUCHWALK;		
		}

		return true;
	}
	
	return false;
}


bool CPortalPlayerAnimState::HandleDying( Activity &idealActivity )
{
	if ( m_bDying )
	{
		if ( m_bFirstDyingFrame )
		{
			// Reset the animation.
			RestartMainSequence();	
			m_bFirstDyingFrame = false;

#ifdef CLIENT_DLL
			//DispatchParticleEffect( "bot_death_B_gib", GetPortalPlayer()->WorldSpaceCenter(), GetPortalPlayer()->GetAbsAngles(), GetPortalPlayer() );
#endif
		}

		if ( GetPortalPlayer()->m_Shared.InCond( PORTAL_COND_DEATH_CRUSH ) )
		{
			idealActivity = ACT_MP_DEATH_CRUSH;
		}
		else
		{
			idealActivity = ACT_DIESIMPLE;
		}

		return true;
	}
	else
	{
		if ( !m_bFirstDyingFrame )
		{
			m_bFirstDyingFrame = true;
		}
	}

	return false;
}


bool CPortalPlayerAnimState::HandleInAir( Activity &idealActivity )
{
	CPortal_Player *pPortalPlayer = GetPortalPlayer();
	if ( pPortalPlayer->GetFlags() & FL_ONGROUND )
	{
		return false;
	}

	if ( m_bWasInTractorBeam || m_bBridgeRemovedFromUnder )
	{
		m_bLanding = true;
		idealActivity = ACT_MP_LONG_FALL;
		return true;
	}
	else
	{
		Vector vecVelocity;
		GetOuterAbsVelocity( vecVelocity );
		if ( vecVelocity.z > 300.0f || m_bInAirWalk )
		{
			// In an air walk.
			m_bJumping = false;
			idealActivity = ACT_MP_AIRWALK;
			m_bInAirWalk = true;
			m_bLanding = true;
			return true;
		}
	}

	return false;
}

ConVar sv_bounce_anim_time_predict( "sv_bounce_anim_time_predict", "0.2", FCVAR_REPLICATED );
ConVar sv_bounce_anim_time_continue( "sv_bounce_anim_time_continue", "0.5", FCVAR_REPLICATED );


bool CPortalPlayerAnimState::HandleBouncing( Activity &idealActivity )
{
	CPortal_Player *pPortalPlayer = GetPortalPlayer();
	float fNextBounceOffsetTime = pPortalPlayer->PredictedBounce();
	bool bPredictedBounce = fNextBounceOffsetTime < sv_bounce_anim_time_predict.GetFloat();
	if ( bPredictedBounce || pPortalPlayer->GetPortalPlayerLocalData().m_fBouncedTime + sv_bounce_anim_time_continue.GetFloat() > gpGlobals->curtime )
	{
		// They're anticipating to hit a bounce soon
		if ( bPredictedBounce )
		{
			pPortalPlayer->OnBounced( fNextBounceOffsetTime );
		}

		m_bJumping = true;
		m_bInAirWalk = true;
		m_bLanding = true;
		idealActivity = ACT_MP_DOUBLEJUMP;
		return true;
	}

	return false;
}


bool CPortalPlayerAnimState::HandleTractorBeam( Activity &idealActivity )
{
	if ( GetPortalPlayer()->GetPortalPlayerLocalData().m_hTractorBeam.Get() )
	{
		if ( m_bFirstTractorBeamFrame )
		{
			RestartMainSequence();
			m_bFirstTractorBeamFrame = false;
		}

		m_bWasInTractorBeam = true;

		idealActivity = ACT_MP_TRACTORBEAM_FLOAT;
		return true;
	}
	else
	{
		if ( !m_bFirstTractorBeamFrame )
		{
			RANDOM_CEG_TEST_SECRET_PERIOD( 8, 15 );
			m_bFirstTractorBeamFrame = true;
		}
	}

	return false;
}


bool CPortalPlayerAnimState::HandleLanding()
{
	// Check to see if we were in the air and now we are basically on the ground or water.
	if ( m_bLanding && GetBasePlayer()->GetFlags() & FL_ONGROUND )
	{
		m_bJumping = false;
		m_bInAirWalk = false;
		m_bLanding = false;
		m_bWasInTractorBeam = false;
		m_bBridgeRemovedFromUnder = false;
		RestartMainSequence();
		RANDOM_CEG_TEST_SECRET_PERIOD( 91, 172 );
		RestartGesture( GESTURE_SLOT_JUMP, ACT_MP_JUMP_LAND );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPortalPlayerAnimState::HandleJumping( Activity &idealActivity )
{
	Vector vecVelocity;
	GetOuterAbsVelocity( vecVelocity );

	// Jumping.
	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		else if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
		{
			// In an air walk.
			m_bJumping = false;
			idealActivity = ACT_MP_AIRWALK;
			m_bInAirWalk = true;
		}

		// if we're still jumping
		if ( m_bJumping )
		{
			idealActivity = ACT_MP_JUMP_START;
		}
	}

	if ( m_bJumping )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPortalPlayerAnimState::SetupPoseParameters( CStudioHdr *pStudioHdr )
{
	CPortal_Player *pPortalPlayer = ToPortalPlayer( GetBasePlayer() );
	if ( pPortalPlayer && ( pPortalPlayer->m_Shared.InCond( PORTAL_COND_TAUNTING ) || pPortalPlayer->m_Shared.InCond( PORTAL_COND_DROWNING ) ) )
		return false;

	return BaseClass::SetupPoseParameters( pStudioHdr );
}


void CPortalPlayerAnimState::IncreaseDamageStage()
{
	if ( m_nDamageStage < DAMAGE_STAGE_FINAL )
	{
		//Disable this for E3
		//m_nDamageStage++;
	}
}
