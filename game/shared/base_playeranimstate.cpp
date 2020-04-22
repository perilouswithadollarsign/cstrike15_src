//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "filesystem.h"


#ifdef CLIENT_DLL
	#include "c_baseplayer.h"
	#include "engine/ivdebugoverlay.h"

	ConVar cl_showanimstate( "cl_showanimstate", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show the (client) animation state for the specified entity (-1 for none)." );
	ConVar showanimstate_log( "cl_showanimstate_log", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "1 to output cl_showanimstate to Msg(). 2 to store in AnimStateClient.log. 3 for both." );
	ConVar showanimstate_activities( "cl_showanimstate_activities", "0", FCVAR_CHEAT, "Show activities in the (client) animation state display." );
#else
	#include "player.h"
	ConVar sv_showanimstate( "sv_showanimstate", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show the (server) animation state for the specified entity (-1 for none)." );
	ConVar showanimstate_log( "sv_showanimstate_log", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "1 to output sv_showanimstate to Msg(). 2 to store in AnimStateServer.log. 3 for both." );
	ConVar showanimstate_activities( "sv_showanimstate_activities", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Show activities in the (server) animation state display." );
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"



// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	15.0f

// After this, need to start turning feet
#define MAX_TORSO_ANGLE		70.0f

// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION		15.0f

#define POSE_PARAM_DELTA_DAMPEN 4.0f

ConVar mp_feetyawrate( 
	"mp_feetyawrate", 
	"400",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"How many degrees per second that we can turn our feet or upper body." );

ConVar mp_facefronttime( 
	"mp_facefronttime", 
	"2", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"After this amount of time of standing in place but aiming to one side, go ahead and move feet to face upper body." );

ConVar mp_ik( "mp_ik", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Use IK on in-place turns." );

// Pose parameters stored for debugging.
float g_flLastBodyPitch, g_flLastBodyYaw, m_flLastMoveYaw;


// ------------------------------------------------------------------------------------------------ //
// CBasePlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //

CBasePlayerAnimState::CBasePlayerAnimState()
{
	m_flEyeYaw = 0.0f;
	m_flEyePitch = 0.0f;
	m_bCurrentFeetYawInitialized = false;
	m_flCurrentTorsoYaw = 0.0f;
	m_flCurrentTorsoYaw = TURN_NONE;
	m_flMaxGroundSpeed = 0.0f;
	m_flStoredCycle = 0.0f;

	m_flGaitYaw = 0.0f;
	m_flGoalFeetYaw = 0.0f;
	m_flCurrentFeetYaw = 0.0f;
	m_bForceAimYaw = false;
	m_flLastYaw = 0.0f;
	m_flLastTurnTime = 0.0f;
	m_angRender.Init();
	m_vLastMovePose.Init();
	m_iCurrent8WayIdleSequence = -1;
	m_iCurrent8WayCrouchIdleSequence = -1;

	m_pOuter = NULL;
	m_eCurrentMainSequenceActivity = ACT_IDLE;
	m_flLastAnimationStateClearTime = 0.0f;
	
	m_bInFootPlantIdleTurn = false;
	m_flFootPlantIdleTurnCycle = 0.0f;
	m_bFootPlantIdleNeedToLiftFeet = false;

	m_flPoseParamTargetDampenedScaleIdeal = 0.0f;
}


CBasePlayerAnimState::~CBasePlayerAnimState()
{
}


void CBasePlayerAnimState::Init( CBaseAnimatingOverlay *pPlayer, const CModAnimConfig &config )
{
	m_pOuter = pPlayer;
	m_AnimConfig = config;
	ClearAnimationState();
}


void CBasePlayerAnimState::Release()
{
	delete this;
}


void CBasePlayerAnimState::ClearAnimationState()
{
	ClearAnimationLayers();
	m_bCurrentFeetYawInitialized = false;
	m_flLastAnimationStateClearTime = gpGlobals->curtime;
}


float CBasePlayerAnimState::TimeSinceLastAnimationStateClear() const
{
	return gpGlobals->curtime - m_flLastAnimationStateClearTime;
}


void CBasePlayerAnimState::Update( float eyeYaw, float eyePitch )
{
	VPROF( "CBasePlayerAnimState::Update" );

	// Clear animation overlays because we're about to completely reconstruct them.
	ClearAnimationLayers();

	// Some mods don't want to update the player's animation state if they're dead and ragdolled.
	if ( !ShouldUpdateAnimState() )
	{
		ClearAnimationState();
		return;
	}
	
	
	CStudioHdr *pStudioHdr = GetOuter()->GetModelPtr();
	// Store these. All the calculations are based on them.
	m_flEyeYaw = AngleNormalize( eyeYaw );
	m_flEyePitch = AngleNormalize( eyePitch );

	// Compute sequences for all the layers.
	ComputeSequences( pStudioHdr );
	
	
	// Compute all the pose params.
	ComputePoseParam_BodyPitch( pStudioHdr );	// Look up/down.
	ComputePoseParam_BodyYaw();		// Torso rotation.
	ComputePoseParam_MoveYaw( pStudioHdr );		// What direction his legs are running in.

	
	ComputePlaybackRate();


#ifdef CLIENT_DLL
	if ( cl_showanimstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimStateFull( 5 );
	}
	else if ( cl_showanimstate.GetInt() == -2 )
	{
		C_BasePlayer *targetPlayer = C_BasePlayer::GetLocalPlayer();

		if( targetPlayer && ( targetPlayer->GetObserverMode() == OBS_MODE_IN_EYE || targetPlayer->GetObserverMode() == OBS_MODE_CHASE ) )
		{
			C_BaseEntity *target = targetPlayer->GetObserverTarget();

			if( target && target->IsPlayer() )
			{
				targetPlayer = ToBasePlayer( target );
			}
		}

		if ( m_pOuter == targetPlayer )
		{
			DebugShowAnimStateFull( 6 );
		}
	}
#else
	if ( sv_showanimstate.GetInt() == m_pOuter->entindex() )
	{
		DebugShowAnimState( 20 );
	}
#endif
}


bool CBasePlayerAnimState::ShouldUpdateAnimState()
{
	// By default, don't update their animation state when they're dead because they're
	// either a ragdoll or they're not drawn.
	return GetOuter()->IsAlive();
}


bool CBasePlayerAnimState::ShouldChangeSequences( void ) const
{
	return true;
}


void CBasePlayerAnimState::SetOuterPoseParameter( int iParam, float flValue )
{
	// Make sure to set all the history values too, otherwise the server can overwrite them.
	GetOuter()->SetPoseParameter( iParam, flValue );
}


void CBasePlayerAnimState::ClearAnimationLayers()
{
	Assert( 0 ); // unused?

	VPROF( "CBasePlayerAnimState::ClearAnimationLayers" );
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( AIMSEQUENCE_LAYER+NUM_AIMSEQUENCE_LAYERS );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#ifndef CLIENT_DLL
		m_pOuter->GetAnimOverlay( i )->m_fFlags = 0;
#endif
	}
}


void CBasePlayerAnimState::RestartMainSequence()
{
	CBaseAnimatingOverlay *pPlayer = GetOuter();

	pPlayer->m_flAnimTime = gpGlobals->curtime;
	pPlayer->SetCycle( 0 );
}


void CBasePlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputeSequences" );

	ComputeMainSequence();		// Lower body (walk/run/idle).
	UpdateInterpolators();		// The groundspeed interpolator uses the main sequence info.

	if ( m_AnimConfig.m_bUseAimSequences )
	{
		ComputeAimSequence();		// Upper body, based on weapon type.
	}
}

void CBasePlayerAnimState::ResetGroundSpeed( void )
{
	m_flMaxGroundSpeed = GetCurrentMaxGroundSpeed();
}

void CBasePlayerAnimState::ComputeMainSequence()
{
	VPROF( "CBasePlayerAnimState::ComputeMainSequence" );

	CBaseAnimatingOverlay *pPlayer = GetOuter();

	// Have our class or the mod-specific class determine what the current activity is.
	Activity idealActivity = CalcMainActivity();

	Activity oldActivity = m_eCurrentMainSequenceActivity;
	
	// Store our current activity so the aim and fire layers know what to do.
	m_eCurrentMainSequenceActivity = idealActivity;

	// Export to our outer class..
	int animDesired = SelectWeightedSequence( TranslateActivity(idealActivity) );

#if !defined ( CSTRIKE_DLL )
	if ( !ShouldResetMainSequence( pPlayer->GetSequence(), animDesired ) )
		return;
#endif

	ResetCycleAcrossCustomActivityChange( oldActivity, idealActivity );

	if ( animDesired < 0 )
		 animDesired = 0;

	pPlayer->ResetSequence( animDesired );

#ifdef CLIENT_DLL
	if ( ShouldResetGroundSpeed( oldActivity, idealActivity ) )
	{
		ResetGroundSpeed();
	}
#endif
}

void CBasePlayerAnimState::ResetCycleAcrossCustomActivityChange( Activity iCurrent, Activity iNew )
{
	if ( !GetOuter() )
		return;

	if ( iCurrent == ACT_CROUCHIDLE && iNew != ACT_CROUCHIDLE )
	{
		GetOuter()->SetCycle(0);
	}
}

bool CBasePlayerAnimState::ShouldResetMainSequence( int iCurrentSequence, int iNewSequence )
{
	if ( !GetOuter() )
		return false;

	return GetOuter()->GetSequenceActivity( iCurrentSequence ) == GetOuter()->GetSequenceActivity( iNewSequence );
}

bool CBasePlayerAnimState::ShouldResetGroundSpeed( Activity oldActivity, Activity idealActivity )
{
	// If we went from idle to walk, reset the interpolation history.
	return ( (oldActivity == ACT_CROUCHIDLE || oldActivity == ACT_IDLE || oldActivity == ACT_TURN || oldActivity == ACT_STEP_FORE ) && 
			 (idealActivity == ACT_WALK || idealActivity == ACT_RUN_CROUCH || idealActivity == ACT_WALK_CROUCH || idealActivity == ACT_RUN ) );
}

void CBasePlayerAnimState::UpdateAimSequenceLayers(
	float flCycle,
	int iFirstLayer,
	bool bForceIdle,
	CSequenceTransitioner *pTransitioner,
	float flWeightScale
	)
{
	float flAimSequenceWeight = 1;
	int iAimSequence = CalcAimLayerSequence( &flCycle, &flAimSequenceWeight, bForceIdle );
	if ( iAimSequence == -1 )
		iAimSequence = 0;

	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );

	pLayer->SetSequence( iAimSequence );
	pLayer->SetCycle( flCycle );
	pLayer->SetWeight( clamp( flWeightScale, 0.0f, 1.0f ) );

	pLayer->SetOrder( iFirstLayer );	// should already be set

#ifndef CLIENT_DLL
	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE;
#endif


}


void CBasePlayerAnimState::OptimizeLayerWeights( int iFirstLayer, int nLayers )
{
	int i;

	// Find the total weight of the blended layers, not including the idle layer (iFirstLayer)
	float totalWeight = 0.0f;
	for ( i=1; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->GetWeight() > 0.0f )
		{
			totalWeight += pLayer->GetWeight();
		}
	}

	// Set the idle layer's weight to be 1 minus the sum of other layer weights
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer );
	if ( pLayer->IsActive() && pLayer->GetWeight() > 0.0f )
	{
		float flWeight = 1.0f - totalWeight;
		flWeight = MAX( flWeight, 0.0f );
		pLayer->SetWeight( flWeight );
	}

	// This part is just an optimization. Since we have the walk/run animations weighted on top of 
	// the idle animations, all this does is disable the idle animations if the walk/runs are at
	// full weighting, which is whenever a guy is at full speed.
	//
	// So it saves us blending a couple animation layers whenever a guy is walking or running full speed.
	int iLastOne = -1;
	for ( i=0; i < nLayers; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
		if ( pLayer->IsActive() && pLayer->GetWeight() > 0.99 )
			iLastOne = i;
	}

	if ( iLastOne != -1 )
	{
		for ( int i=iLastOne-1; i >= 0; i-- )
		{
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iFirstLayer+i );
#ifdef CLIENT_DLL 
			pLayer->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#else
			pLayer->m_nOrder.Set( CBaseAnimatingOverlay::MAX_OVERLAYS );
			pLayer->m_fFlags = 0;
#endif
		}
	}
}

bool CBasePlayerAnimState::ShouldBlendAimSequenceToIdle()
{
	Activity act = GetCurrentMainSequenceActivity();

	return (act == ACT_RUN || act == ACT_LEAP || act == ACT_WALK || act == ACT_JUMP || act == ACT_RUNTOIDLE || act == ACT_RUN_CROUCH || act == ACT_WALK_CROUCH );
}

void CBasePlayerAnimState::ComputeAimSequence()
{
	VPROF( "CBasePlayerAnimState::ComputeAimSequence" );

	// Synchronize the lower and upper body cycles.
	float flCycle = m_pOuter->GetCycle();

	//[msmith]
	bool bIsMoving = false;
	float flPlaybackRate = 0.0f;

	if ( ShouldBlendAimSequenceToIdle() )
	{
		// See how much the player should weigh in the motion vs. the idle layers.
		flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );
	}

	// Set the idle layers to full (blend between different idle layers only and do NOT consider movement layers).
	UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER, true, &m_HighAimSequenceTransitioner, 1.0f );

	if ( bIsMoving )
	{
		// Blend the run / walk layers together.
		UpdateAimSequenceLayers( flCycle, AIMSEQUENCE_LAYER+1, false, &m_LowAimSequenceTransitioner, flPlaybackRate );
	}

	OptimizeLayerWeights( AIMSEQUENCE_LAYER, NUM_AIMSEQUENCE_LAYERS );
}


int CBasePlayerAnimState::CalcSequenceIndex( PRINTF_FORMAT_STRING const char *pBaseName, ... )
{
	char szFullName[512];
	va_list marker;
	va_start( marker, pBaseName );
	Q_vsnprintf( szFullName, sizeof( szFullName ), pBaseName, marker );
	va_end( marker );
	int iSequence = GetOuter()->LookupSequence( szFullName );
	
	// Show warnings if we can't find anything here.
	if ( iSequence == -1 )
	{
		static CUtlDict<int,int> dict;
		if ( dict.Find( szFullName ) == dict.InvalidIndex() )
		{
			dict.Insert( szFullName, 0 );
			Warning( "CalcSequenceIndex: can't find '%s'.\n", szFullName );
		}

		iSequence = 0;
	}

	return iSequence;
}



void CBasePlayerAnimState::UpdateInterpolators()
{
	VPROF( "CBasePlayerAnimState::UpdateInterpolators" );

	// First, figure out their current max speed based on their current activity.
	float flCurMaxSpeed = GetCurrentMaxGroundSpeed();
	m_flMaxGroundSpeed = flCurMaxSpeed;
}


float CBasePlayerAnimState::GetInterpolatedGroundSpeed()
{
	return m_flMaxGroundSpeed;
}


float CBasePlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();
	bool isMoving = ( speed > MOVING_MINIMUM_SPEED );

	*bIsMoving = false;
	float flReturnValue = 1;

	if ( isMoving && CanThePlayerMove() )
	{
		float flGroundSpeed = GetInterpolatedGroundSpeed();
		if ( flGroundSpeed < 0.001f )
		{
			flReturnValue = 0.01;
		}
		else
		{
			flReturnValue = speed / flGroundSpeed;
			flReturnValue = clamp( flReturnValue, 0.01, 10 );	// don't go nuts here.
		}
		*bIsMoving = true;
	}
	
	return flReturnValue;
}


bool CBasePlayerAnimState::CanThePlayerMove()
{
	return true;
}


void CBasePlayerAnimState::ComputePlaybackRate()
{
	VPROF( "CBasePlayerAnimState::ComputePlaybackRate" );
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
	{
		// When using a 9-way blend, playback rate is always 1 and we just scale the pose params
		// to speed up or slow down the animation.
		bool bIsMoving;
		float flRate = CalcMovementPlaybackRate( &bIsMoving );
		if ( bIsMoving )
			GetOuter()->SetPlaybackRate( flRate );
		else
			GetOuter()->SetPlaybackRate( 1 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBasePlayer
//-----------------------------------------------------------------------------
CBaseAnimatingOverlay *CBasePlayerAnimState::GetOuter() const
{
	return m_pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::EstimateYaw()
{
	Vector est_velocity;
	GetOuterAbsVelocity( est_velocity );

	float flLength = est_velocity.Length2D();
	if ( flLength > MOVING_MINIMUM_SPEED )
	{
		m_flGaitYaw = atan2( est_velocity[1], est_velocity[0] );
		m_flGaitYaw = RAD2DEG( m_flGaitYaw );
		m_flGaitYaw = AngleNormalize( m_flGaitYaw );
	}
}

#define MOVEMENT_MINIMUM_ANIMATED_SPEED 10.0f
#define MOVEMENT_MAXIMUM_PLAYBACK_RATE 1.3f

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt - 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_MoveYaw( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_MoveYaw" );

	//Matt: Goldsrc style animations need to not rotate the model
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_GOLDSRC )
	{
#ifndef CLIENT_DLL
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );
#endif
	}

	// If using goldsrc-style animations where he's moving in the direction that his feet are facing,
	// we don't use move yaw.
	if ( m_AnimConfig.m_LegAnimType != LEGANIM_9WAY && m_AnimConfig.m_LegAnimType != LEGANIM_8WAY )
		return;

	// view direction relative to movement
	float flYaw;	 

	EstimateYaw();

	float ang = m_flEyeYaw;
	if ( ang > 180.0f )
	{
		ang -= 360.0f;
	}
	else if ( ang < -180.0f )
	{
		ang += 360.0f;
	}

	// calc side to side turning
	flYaw = ang - m_flGaitYaw;
	// Invert for mapping into 8way blend
	flYaw = -flYaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if (flYaw < -180)
	{
		flYaw = flYaw + 360;
	}
	else if (flYaw > 180)
	{
		flYaw = flYaw - 360;
	}

	
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY )
	{
		//Adrian: Make the model's angle match the legs so the hitboxes match on both sides.
		//[msmith]: Since bounding box code uses the entities local angle, we need the local angle to match
		//			the render angle on both the server AND the client.  The server for hit tests, the client
		//			for proper visibility culling.
		GetOuter()->SetLocalAngles( QAngle( 0, m_flCurrentFeetYaw, 0 ) );

		int iMoveX = GetOuter()->LookupPoseParameter( pStudioHdr, "move_x" );
		int iMoveY = GetOuter()->LookupPoseParameter( pStudioHdr, "move_y" );
		if ( iMoveX < 0 || iMoveY < 0 )
			return;

		bool bIsMoving;
		float flPlaybackRate = CalcMovementPlaybackRate( &bIsMoving );

#ifdef CLIENT_DLL
		Vector vel;
		GetOuterAbsVelocity( vel );
		bIsMoving = ( vel.Length2D() > 0 );
#endif

		// Setup the 9-way blend parameters based on our speed and direction.
		Vector2D vCurMovePose( 0, 0 );

		m_flPoseParamTargetDampenedScaleIdeal = Approach( flPlaybackRate, m_flPoseParamTargetDampenedScaleIdeal, gpGlobals->frametime * POSE_PARAM_DELTA_DAMPEN );

		if ( bIsMoving )
		{
			vCurMovePose.x = cos( DEG2RAD( flYaw ) );
			vCurMovePose.y = -sin( DEG2RAD( flYaw ) );
			// movement pose parameters on the diagonals are encoded at 1 instead of 0.707 (cos 45)
			// scale to the outside of a box instead of the outside of a circle.
			float scale = fabs( vCurMovePose.x );
			float scale2 = fabs( vCurMovePose.y );
			if ( scale2 > scale ) scale = scale2;
			if ( scale > 0.01f )
			{
				scale = 1.0f / scale;
				vCurMovePose.x *= scale;
				vCurMovePose.y *= scale;
			}

#ifdef CLIENT_DLL
			// find the max speed the animation will move in the current direction

			//If these aren't applied here, the legs stutter. Even though they're re-applied in a sec
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, vCurMovePose.x );
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, vCurMovePose.y );

			Vector vecAnimatedVel;
			GetOuter()->GetBlendedLinearVelocity( &vecAnimatedVel );
			float flAnimatedSpeed = vecAnimatedVel.Length2D();

			// find how to scale the current animation down to the desired speed
			Vector vel;
			GetOuterAbsVelocity( vel );
			float flMovementSpeed = vel.Length2D();

			if ( flAnimatedSpeed > CS_PLAYER_SPEED_RUN )
				flAnimatedSpeed = flMovementSpeed;

			if ( flAnimatedSpeed < MOVEMENT_MINIMUM_ANIMATED_SPEED )
			{
				// we're moving so slowly (either just starting or just stopping) that current playback rate is almost nothing.
				flPlaybackRate = flMovementSpeed / ( MOVEMENT_MINIMUM_ANIMATED_SPEED * 2.0f );
			}
			else
			{
				
				flPlaybackRate = flMovementSpeed / flAnimatedSpeed;
			}

			// player is moving less than what's animated, scale pose parameters back towards 0,0
			if ( flPlaybackRate < 1.0f )
			{
				if ( flPlaybackRate > 0.08f )
				{
					vCurMovePose.x *= flPlaybackRate;
					vCurMovePose.y *= flPlaybackRate;
					GetOuter()->SetPlaybackRate( flPlaybackRate );
				}
				else
				{
					vCurMovePose.x *= 0.08f;
					vCurMovePose.y *= 0.08f;
					GetOuter()->SetPlaybackRate( 1.0f );
				}
			}
			else
			{
				// speed up the animation to match the needed motion, but only so far
				flPlaybackRate = clamp( flPlaybackRate, 1.0f, MOVEMENT_MAXIMUM_PLAYBACK_RATE );
				GetOuter()->SetPlaybackRate( flPlaybackRate );
			}
#endif
			vCurMovePose.x *= m_flPoseParamTargetDampenedScaleIdeal;
			vCurMovePose.y *= m_flPoseParamTargetDampenedScaleIdeal;

		}



		GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, vCurMovePose.x );
		GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, vCurMovePose.y );

		m_vLastMovePose = vCurMovePose;
	}
	else
	{
		int iMoveYaw = GetOuter()->LookupPoseParameter( pStudioHdr, "move_yaw" );
		if ( iMoveYaw >= 0 )
		{
			GetOuter()->SetPoseParameter( pStudioHdr, iMoveYaw, flYaw );
			m_flLastMoveYaw = flYaw;

			// Now blend in his idle animation.
			// This makes the 8-way blend act like a 9-way blend by blending to 
			// an idle sequence as he slows down.
#if defined(CLIENT_DLL)
#ifndef INFESTED_DLL
			bool bIsMoving;
			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );
			
			pLayer->SetWeight( 1 - CalcMovementPlaybackRate( &bIsMoving ) );
			if ( !bIsMoving )
			{
				pLayer->SetWeight( 1 );
			}

			if ( ShouldChangeSequences() )
			{
				// Whenever this layer stops blending, we can choose a new idle sequence to blend to, so he 
				// doesn't always use the same idle.
				if ( pLayer->GetWeight() < 0.02f || m_iCurrent8WayIdleSequence == -1 )
				{
					m_iCurrent8WayIdleSequence = m_pOuter->SelectWeightedSequence( ACT_IDLE );
					m_iCurrent8WayCrouchIdleSequence = m_pOuter->SelectWeightedSequence( ACT_CROUCHIDLE );
				}

				if ( m_eCurrentMainSequenceActivity == ACT_CROUCHIDLE || m_eCurrentMainSequenceActivity == ACT_RUN_CROUCH )
					pLayer->SetSequence( m_iCurrent8WayCrouchIdleSequence );
				else
					pLayer->SetSequence( m_iCurrent8WayIdleSequence );
			}
			
			pLayer->SetPlaybackRate( 1 );
			pLayer->SetCycle( pLayer->GetCycle() + m_pOuter->GetSequenceCycleRate( pStudioHdr, pLayer->GetSequence() ) * gpGlobals->frametime );
			pLayer->SetCycle( fmod( pLayer->GetCycle(), 1 ) );
			pLayer->SetOrder( MAIN_IDLE_SEQUENCE_LAYER );
#endif
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_BodyPitch" );

	// Get pitch from v_angle
	float flPitch = m_flEyePitch;
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "body_pitch" );
	if ( pitch < 0 )
		return;

	GetOuter()->SetPoseParameter( pStudioHdr, pitch, flPitch );
	g_flLastBodyPitch = flPitch;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : goal - 
//			maxrate - 
//			dt - 
//			current - 
// Output : int
//-----------------------------------------------------------------------------
int CBasePlayerAnimState::ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	anglediff = AngleNormalize( anglediff );
	
	float anglediffabs = fabs( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( anglediffabs > maxgap )
	{
		// gap is too big, jump
		//maxmove = (anglediffabs - maxgap);
		float flTooFar = MIN( anglediffabs - maxgap, maxmove * 5 );
		if ( anglediff > 0 )
		{
			current += flTooFar;
		}
		else
		{
			current -= flTooFar;
		}
		current = AngleNormalize( current );
		anglediff = goal - current;
		anglediff = AngleNormalize( anglediff );
		anglediffabs = fabs( anglediff );
	}

	if ( anglediffabs < maxmove )
	{
		// we are close enought, just set the final value
		current = goal;
	}
	else
	{
		// adjust value up or down
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

void CBasePlayerAnimState::ComputePoseParam_BodyYaw()
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_BodyYaw" );

	// Find out which way he's running (m_flEyeYaw is the way he's looking).
	Vector vel;
	GetOuterAbsVelocity( vel );
	bool bIsMoving = vel.Length2D() > MOVING_MINIMUM_SPEED;

	// If we just initialized this guy (maybe he just came into the PVS), then immediately
	// set his feet in the right direction, otherwise they'll spin around from 0 to the 
	// right direction every time someone switches spectator targets.
	if ( !m_bCurrentFeetYawInitialized )
	{
		m_bCurrentFeetYawInitialized = true;
		m_flGoalFeetYaw = m_flCurrentFeetYaw = m_flEyeYaw;
		m_flLastTurnTime = 0.0f;
		m_bInFootPlantIdleTurn = false;
	}
	else if ( bIsMoving || m_bForceAimYaw )
	{
		// player is moving, feet yaw = aiming yaw
		if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY || m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
		{
			// His feet point in the direction his eyes are, but they can run in any direction.
			m_flGoalFeetYaw = m_flEyeYaw;
		}
		else
		{
			m_flGoalFeetYaw = RAD2DEG( atan2( vel.y, vel.x ) );

			// If he's running backwards, flip his feet backwards.
			Vector vEyeYaw( cos( DEG2RAD( m_flEyeYaw ) ), sin( DEG2RAD( m_flEyeYaw ) ), 0 );
			Vector vFeetYaw( cos( DEG2RAD( m_flGoalFeetYaw ) ), sin( DEG2RAD( m_flGoalFeetYaw ) ), 0 );
			if ( vEyeYaw.Dot( vFeetYaw ) < -0.01 )
			{
				m_flGoalFeetYaw += 180;
			}
		}
		m_bInFootPlantIdleTurn = false;
	}
	else if ( (gpGlobals->curtime - m_flLastTurnTime) > mp_facefronttime.GetFloat() && m_flGoalFeetYaw != m_flEyeYaw )
	{
		// player didn't move & turn for quite some time
		if ( vel.Length2D() <= FOOTPLANT_MINIMUM_SPEED )
		{
			m_bInFootPlantIdleTurn = true;
			if ( m_flFootPlantIdleTurnCycle >= 1 )
				m_flFootPlantIdleTurnCycle = 0;
		}

		float flDiff = AngleNormalize(m_flGoalFeetYaw - m_flEyeYaw);
		m_bFootPlantIdleNeedToLiftFeet = (fabs(flDiff) > m_AnimConfig.m_flIdleFootPlantFootLiftDelta);

		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else
	{
		// If he's rotated his view further than the model can turn, make him face forward.
		float flDiff = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

		if ( fabs(flDiff) > m_AnimConfig.m_flMaxBodyYawDegrees )
		{
			if ( vel.Length2D() <= FOOTPLANT_MINIMUM_SPEED )
			{
				m_bInFootPlantIdleTurn = true;
				if ( m_flFootPlantIdleTurnCycle >= 1 )
					m_flFootPlantIdleTurnCycle = 0;
			}

			m_bFootPlantIdleNeedToLiftFeet = true;

			if ( flDiff  > 0 )
				m_flGoalFeetYaw -= m_AnimConfig.m_flMaxBodyYawDegreesCorrectionAmount;
			else
				m_flGoalFeetYaw += m_AnimConfig.m_flMaxBodyYawDegreesCorrectionAmount;
		}

		// If current yaw is significantly different from goal, abort idle foot ik to avoid intersecting the legs
		if ( m_bInFootPlantIdleTurn )
		{
			float flDiffYaw = AngleNormalize(m_flCurrentFeetYaw - m_flGoalFeetYaw);
			if ( fabs(flDiffYaw) > m_AnimConfig.m_flIdleFootPlantMaxYaw )
			{
				m_bInFootPlantIdleTurn = false;
				m_flFootPlantIdleTurnCycle = 0;
			}
		}
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );

	if ( m_flCurrentFeetYaw != m_flGoalFeetYaw )
	{
		if ( m_bForceAimYaw )
		{
			m_flCurrentFeetYaw = m_flGoalFeetYaw;
		}
		else
		{
			ConvergeAngles( m_flGoalFeetYaw, GetFeetYawRate(), m_AnimConfig.m_flMaxBodyYawDegrees, gpGlobals->frametime, m_flCurrentFeetYaw );
		}

		m_flLastTurnTime = gpGlobals->curtime;
	}

	// Turn off a force aim yaw - either we have already updated or we don't need to.
	m_bForceAimYaw = false;

	float flCurrentTorsoYaw = AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );

	// Rotate entire body into position
	m_angRender[YAW] = m_flCurrentFeetYaw;
	m_angRender[PITCH] = m_angRender[ROLL] = 0;
		
	SetOuterBodyYaw( flCurrentTorsoYaw );
	g_flLastBodyYaw = flCurrentTorsoYaw;
}



float CBasePlayerAnimState::SetOuterBodyYaw( float flValue )
{
	int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );
	if ( body_yaw < 0 )
	{
		return 0;
	}

	SetOuterPoseParameter( body_yaw, flValue );
	return flValue;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CBasePlayerAnimState::BodyYawTranslateActivity( Activity activity )
{
	// Not even standing still, sigh
	if ( activity != ACT_IDLE )
		return activity;

	// Not turning
	switch ( m_nTurningInPlace )
	{
	default:
	case TURN_NONE:
		return activity;
	case TURN_RIGHT:
	case TURN_LEFT:
		return mp_ik.GetBool() ? ACT_TURN : activity;
	}

	Assert( 0 );
	return activity;
}

const QAngle& CBasePlayerAnimState::GetRenderAngles()
{
	return m_angRender;
}

void CBasePlayerAnimState::SetForceAimYaw( bool bForce )
{
	m_bForceAimYaw = bForce;
}


void CBasePlayerAnimState::GetOuterAbsVelocity( Vector& vel ) const
{
#if defined( CLIENT_DLL )
	GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}


float CBasePlayerAnimState::GetOuterXYSpeed() const
{
	Vector vel;
	GetOuterAbsVelocity( vel );
	return vel.Length2D();
}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStateLog( PRINTF_FORMAT_STRING const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Log it?	
	if ( showanimstate_log.GetInt() == 1 || showanimstate_log.GetInt() == 3 )
	{
		Msg( "%s", str );
	}

	if ( showanimstate_log.GetInt() > 1 )
	{
#ifdef CLIENT_DLL
		const char *fname = "AnimStateClient.log";
#else
		const char *fname = "AnimStateServer.log";
#endif
		static FileHandle_t hFile = filesystem->Open( fname, "wt" );
		filesystem->FPrintf( hFile, "%s", str );
		filesystem->Flush( hFile );
	}
}


// -----------------------------------------------------------------------------
void CBasePlayerAnimState::AnimStatePrintf( int iLine, PRINTF_FORMAT_STRING const char *pMsg, ... )
{
	// Format the string.
	char str[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	// Show it with Con_NPrintf.
	engine->Con_NPrintf( iLine, "%s", str );

	// Log it.
	AnimStateLog( "%s\n", str );
}


// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimState( int iStartLine )
{
	Vector vOuterVel;
	GetOuterAbsVelocity( vOuterVel );

	int iLine = iStartLine;
	AnimStatePrintf( iLine++, "main: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
		GetSequenceName( m_pOuter->GetModelPtr(), m_pOuter->GetSequence() ), 
		m_pOuter->GetSequence(),
		m_pOuter->GetCycle(), 
		m_pOuter->GetSequenceCycleRate(m_pOuter->GetModelPtr(), m_pOuter->GetSequence()),
		m_pOuter->GetPlaybackRate()
		);

	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( MAIN_IDLE_SEQUENCE_LAYER );

		AnimStatePrintf( iLine++, "idle: %s, weight: %.2f\n",
			GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ), 
			(float)pLayer->GetWeight() );
	}

	for ( int i=0; i < m_pOuter->GetNumAnimOverlays()-1; i++ )
	{
		CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( AIMSEQUENCE_LAYER + i );
#ifdef CLIENT_DLL
		AnimStatePrintf( iLine++, "%s(%d), weight: %.2f, cycle: %.2f, order (%d), aim (%d)", 
			!pLayer->IsActive() ? "-- ": (pLayer->GetSequence() == 0 ? "-- " : (showanimstate_activities.GetBool()) ? GetSequenceActivityName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) : GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) ), 
			!pLayer->IsActive() ? 0 : (int)pLayer->GetSequence(), 
			!pLayer->IsActive() ? 0 : (float)pLayer->GetWeight(), 
			!pLayer->IsActive() ? 0 : (float)pLayer->GetCycle(), 
			!pLayer->IsActive() ? 0 : (int)pLayer->GetOrder(),
			i
			);
#else
		AnimStatePrintf( iLine++, "%s(%d), flags (%d), weight: %.2f, cycle: %.2f, order (%d), aim (%d)", 
			!pLayer->IsActive() ? "-- " : ( pLayer->GetSequence() == 0 ? "-- " : (showanimstate_activities.GetBool()) ? GetSequenceActivityName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) : GetSequenceName( m_pOuter->GetModelPtr(), pLayer->GetSequence() ) ), 
			!pLayer->IsActive() ? 0 : (int)pLayer->GetSequence(), 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_fFlags,// Doesn't exist on client
			!pLayer->IsActive() ? 0 : (float)pLayer->GetWeight(), 
			!pLayer->IsActive() ? 0 : (float)pLayer->GetCycle(), 
			!pLayer->IsActive() ? 0 : (int)pLayer->m_nOrder,
			i
			);
#endif
	}

	AnimStatePrintf( iLine++, "vel: %.2f, time: %.2f, max: %.2f, animspeed: %.2f", 
		vOuterVel.Length2D(), gpGlobals->curtime, GetInterpolatedGroundSpeed(), m_pOuter->GetSequenceGroundSpeed(m_pOuter->GetSequence()) );
	
	if ( m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, move_yaw: %.2f, gait_yaw: %.2f, body_pitch: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, m_flLastMoveYaw, m_flGaitYaw, g_flLastBodyPitch );
	}
	else
	{
		AnimStatePrintf( iLine++, "ent yaw: %.2f, body_yaw: %.2f, body_pitch: %.2f, move_x: %.2f, move_y: %.2f", 
			m_angRender[YAW], g_flLastBodyYaw, g_flLastBodyPitch, m_vLastMovePose.x, m_vLastMovePose.y );
	}

	// Draw a red triangle on the ground for the eye yaw.
	float flBaseSize = 10;
	float flHeight = 80;
	Vector vBasePos = GetOuter()->GetAbsOrigin() + Vector( 0, 0, 3 );
	QAngle angles( 0, 0, 0 );
	angles[YAW] = m_flEyeYaw;
	Vector vForward, vRight, vUp;
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 255, false, gpGlobals->frametime );

	// Draw a blue triangle on the ground for the body yaw.
	angles[YAW] = m_angRender[YAW];
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 0, 255, 255, false, gpGlobals->frametime );

	
	//left limit
	angles[YAW] = m_flEyeYaw - 60;
	AngleVectors(angles, &vForward, &vRight, &vUp);
	debugoverlay->AddTriangleOverlay(vBasePos + vRight*flBaseSize / 4, vBasePos - vRight*flBaseSize / 4, vBasePos + vForward*flHeight, 0, 255, 0, 100, false, gpGlobals->frametime);

	//right limit
	angles[YAW] = m_flEyeYaw + 60;
	AngleVectors(angles, &vForward, &vRight, &vUp);
	debugoverlay->AddTriangleOverlay(vBasePos + vRight*flBaseSize / 4, vBasePos - vRight*flBaseSize / 4, vBasePos + vForward*flHeight, 0, 255, 0, 100, false, gpGlobals->frametime);


}

// -----------------------------------------------------------------------------
void CBasePlayerAnimState::DebugShowAnimStateFull( int iStartLine )
{
	AnimStateLog( "----------------- frame %d -----------------\n", gpGlobals->framecount );

	DebugShowAnimState( iStartLine );

	AnimStateLog( "--------------------------------------------\n\n" );
}

// -----------------------------------------------------------------------------
int CBasePlayerAnimState::SelectWeightedSequence( Activity activity ) 
{
	return GetOuter()->SelectWeightedSequence( activity ); 
}

float CBasePlayerAnimState::GetFeetYawRate( void )
{
	return mp_feetyawrate.GetFloat();
}
