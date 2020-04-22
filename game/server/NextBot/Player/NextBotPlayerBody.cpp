// NextBotPlayerBody.cpp
// Implementation of Body interface for CBasePlayer-derived classes
// Author: Michael Booth, October 2006
// Copyright (c) 2006 Turtle Rock Studios, Inc. - All Rights Reserved

#include "cbase.h"

#include "NextBot.h"
#include "NextBotPlayerBody.h"
#include "NextBotPlayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar NextBotSaccadeTime( "nb_saccade_time", "0.1", FCVAR_CHEAT );
ConVar NextBotSaccadeSpeed( "nb_saccade_speed", "1000", FCVAR_CHEAT );
ConVar NextBotHeadAimSteadyMaxRate( "nb_head_aim_steady_max_rate", "100", FCVAR_CHEAT );
ConVar NextBotHeadAimSettleDuration( "nb_head_aim_settle_duration", "0.3", FCVAR_CHEAT );


//-----------------------------------------------------------------------------------------------
/** 
 * A useful reply for IBody::AimHeadTowards.  When the
 * head is aiming on target, press the fire button.
 */
void PressFireButtonReply::OnSuccess( INextBot *bot )
{
	INextBotPlayerInput *playerInput = dynamic_cast< INextBotPlayerInput * >( bot->GetEntity() );
	if ( playerInput )
	{
		playerInput->PressFireButton();
	}
}


//-----------------------------------------------------------------------------------------------
/** 
 * A useful reply for IBody::AimHeadTowards.  When the
 * head is aiming on target, press the alternate fire button.
 */
void PressAltFireButtonReply::OnSuccess( INextBot *bot )
{
	INextBotPlayerInput *playerInput = dynamic_cast< INextBotPlayerInput * >( bot->GetEntity() );
	if ( playerInput )
	{
		playerInput->PressMeleeButton();
	}
}


//-----------------------------------------------------------------------------------------------
/** 
 * A useful reply for IBody::AimHeadTowards.  When the
 * head is aiming on target, press the jump button.
 */
void PressJumpButtonReply::OnSuccess( INextBot *bot )
{
	INextBotPlayerInput *playerInput = dynamic_cast< INextBotPlayerInput * >( bot->GetEntity() );
	if ( playerInput )
	{
		playerInput->PressJumpButton();
	}
}


//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
PlayerBody::PlayerBody( INextBot *bot ) : IBody( bot )
{
	m_player = static_cast< CBasePlayer * >( bot->GetEntity() );
}


//-----------------------------------------------------------------------------------------------
PlayerBody::~PlayerBody()
{
}


//-----------------------------------------------------------------------------------------------
/**
 * reset to initial state
 */
void PlayerBody::Reset( void )
{
	m_posture = STAND;

	m_lookAtPos = vec3_origin;
	m_lookAtSubject = NULL;
	m_lookAtReplyWhenAimed = NULL;

	m_lookAtPriority = BORING;
	m_lookAtExpireTimer.Invalidate();
	m_lookAtDurationTimer.Invalidate();
	m_isSightedIn = false;
	m_hasBeenSightedIn = false;
	m_headSteadyTimer.Invalidate();
	m_yawRate = 0.0f;
	m_pitchRate = 0.0f;
	m_priorAngles = vec3_angle;
}

static ConVar bot_mimic( "bot_mimic", "0", 0, "Bot uses usercmd of player by index." );

//-----------------------------------------------------------------------------------------------
/**
 * Update internal state.
 * Do this every tick to keep head aims smooth and accurate
 */
void PlayerBody::Upkeep( void )
{
	// If mimicking the player, don't modify the view angles.
	static ConVarRef bot_mimic( "bot_mimic" );
	if ( bot_mimic.IsValid() && bot_mimic.GetBool() )
		return;

	const float deltaT = gpGlobals->frametime;
	if ( deltaT < 0.00001f )
	{
		return;
	}

	CBasePlayer *player = ( CBasePlayer * )GetBot()->GetEntity();

	// get current view angles
	QAngle currentAngles = player->EyeAngles() + player->GetPunchAngle();


	// track when our head is "steady"
	bool isSteady = true;

	float actualPitchRate = AngleDiff( currentAngles.x, m_priorAngles.x );
	if ( abs( actualPitchRate ) > NextBotHeadAimSteadyMaxRate.GetFloat() * deltaT )
	{
		isSteady = false;
	}
	else
	{
		float actualYawRate = AngleDiff( currentAngles.y, m_priorAngles.y );

		if ( abs( actualYawRate ) > NextBotHeadAimSteadyMaxRate.GetFloat() * deltaT )
		{
			isSteady = false;
		}
	}

	if ( isSteady )
	{
		if ( !m_headSteadyTimer.HasStarted() )
		{
			m_headSteadyTimer.Start();
		}
	}
	else
	{
		m_headSteadyTimer.Invalidate();
	}

	if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
	{
		if ( IsHeadSteady() )
		{
			const float maxTime = 3.0f;
			float t = GetHeadSteadyDuration() / maxTime;
			t = clamp( t, 0, 1.0f );
			NDebugOverlay::Circle( player->EyePosition(), t * 10.0f, 0, 255, 0, 255, true, 2.0f * deltaT );
		}
	}

	m_priorAngles = currentAngles;


	// if our current look-at has expired, don't change our aim further
	if ( m_hasBeenSightedIn && m_lookAtExpireTimer.IsElapsed() )
	{
		return;
	}


	// if we have a subject, update lookat point
	CBaseEntity *subject = m_lookAtSubject;
	if ( subject )
	{
		if ( subject->MyCombatCharacterPointer() ) 
		{
			m_lookAtPos = GetBot()->GetIntentionInterface()->SelectTargetPoint( GetBot(), subject->MyCombatCharacterPointer() );
		}
		else
		{
			m_lookAtPos = subject->WorldSpaceCenter();
		}

		m_lookAtPos += GetHeadAimSubjectLeadTime() * subject->GetAbsVelocity();
	}

	// aim view towards last look at point
	Vector to = m_lookAtPos - GetEyePosition();
	to.NormalizeInPlace();

	QAngle desiredAngles;
	VectorAngles( to, desiredAngles );

	QAngle angles;

	const Vector &forward = GetViewVector();

	if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
	{
		NDebugOverlay::Line( GetEyePosition(), GetEyePosition() + 100.0f * forward, 255, 255, 0, false, 2.0f * deltaT );

		float thickness = isSteady ? 2.0f : 3.0f;
		int g = subject ? 255 : 0;
		NDebugOverlay::HorzArrow( GetEyePosition(), m_lookAtPos, thickness, 0, g, 255, 255, false, 2.0f * deltaT );
	}

	
	const float onTargetTolerance = 0.98f;
	float dot = DotProduct( forward, to );
	if ( dot > onTargetTolerance )
	{
		// on target
		m_isSightedIn = true;
		m_hasBeenSightedIn = true;

		if ( m_lookAtReplyWhenAimed )
		{
			m_lookAtReplyWhenAimed->OnSuccess( GetBot() );
			m_lookAtReplyWhenAimed = NULL;
		}
	}
	else
	{
		// off target
		m_isSightedIn = false;
	}


	// rotate view at a rate proportional to how far we have to turn
	// max rate if we need to turn around
	// want first derivative continuity of rate as our aim hits to avoid pop
	float approachRate = GetMaxHeadAngularVelocity();

	const float easeOut = 0.7f;
	if ( dot > easeOut )
	{
		float t = RemapVal( dot, easeOut, 1.0f, 1.0f, 0.02f );
		const float halfPI = 1.57f;
		approachRate *= sin( halfPI * t );
	}

	const float easeInTime = 0.25f;
	if ( m_lookAtDurationTimer.GetElapsedTime() < easeInTime )
	{
		approachRate *= m_lookAtDurationTimer.GetElapsedTime() / easeInTime;
	}

	angles.y = ApproachAngle( desiredAngles.y, currentAngles.y, approachRate * deltaT );
	angles.x = ApproachAngle( desiredAngles.x, currentAngles.x, approachRate * deltaT );
	angles.z = 0.0f;

	// back out "punch angle"
	angles -= player->GetPunchAngle();

	angles.x = AngleNormalize( angles.x );
	angles.y = AngleNormalize( angles.y );

	player->SnapEyeAngles( angles );
}


//-----------------------------------------------------------------------------------------------
bool PlayerBody::SetPosition( const Vector &pos )
{
	m_player->SetAbsOrigin( pos );	
	return true;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return the eye position of the bot in world coordinates
 */
const Vector &PlayerBody::GetEyePosition( void ) const
{
	m_eyePos = m_player->EyePosition();
	return m_eyePos;
}


CBaseEntity *PlayerBody::GetEntity( void )
{
	return m_player;
}

//-----------------------------------------------------------------------------------------------
/**
 * Return the view unit direction vector in world coordinates
 */
const Vector &PlayerBody::GetViewVector( void ) const
{
	m_player->EyeVectors( &m_viewVector );
	return m_viewVector;
}


//-----------------------------------------------------------------------------------------------
/**
 * Aim the bot's head towards the given goal
 */
void PlayerBody::AimHeadTowards( const Vector &lookAtPos, LookAtPriorityType priority, float duration, INextBotReply *replyWhenAimed, const char *reason )
{
	if ( duration <= 0.0f )
	{
		duration = 0.1f;
	}

	// don't spaz our aim around
	if ( m_lookAtPriority == priority )
	{
		if ( !IsHeadSteady() || GetHeadSteadyDuration() < NextBotHeadAimSettleDuration.GetFloat() )
		{
			// we're still finishing a look-at at the same priority
			if ( replyWhenAimed ) 
			{
				replyWhenAimed->OnFail( GetBot(), INextBotReply::DENIED );
			}

			if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
			{
				ConColorMsg( Color( 255, 0, 0, 255 ), "%3.2f: %s Look At rejected - previous aim not settled\n",
								gpGlobals->curtime,
								m_player->GetPlayerName() );
			}
			return;
		}
	}

	// don't short-circuit if "sighted in" to avoid rapid view jitter
	if ( m_lookAtPriority > priority && !m_lookAtExpireTimer.IsElapsed() )
	{
		// higher priority lookat still ongoing 
		if ( replyWhenAimed ) 
		{
			replyWhenAimed->OnFail( GetBot(), INextBotReply::DENIED );
		}

		if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
		{
			ConColorMsg( Color( 255, 0, 0, 255 ), "%3.2f: %s Look At rejected - higher priority aim in progress\n",
							gpGlobals->curtime,
							m_player->GetPlayerName() );
		}
		return;
	}

	if ( m_lookAtReplyWhenAimed )
	{
		// in-process aim was interrupted
		m_lookAtReplyWhenAimed->OnFail( GetBot(), INextBotReply::INTERRUPTED );
	}

	m_lookAtReplyWhenAimed = replyWhenAimed;
	m_lookAtExpireTimer.Start( duration );

	// if given the same point, just update priority
	const float epsilon = 1.0f;
	if ( ( m_lookAtPos - lookAtPos ).IsLengthLessThan( epsilon ) )
	{
		m_lookAtPriority = priority;
		return;
	}

	// new look-at point

	m_lookAtPos = lookAtPos;
	m_lookAtSubject = NULL;

	m_lookAtPriority = priority;
	m_lookAtDurationTimer.Start();
	m_isSightedIn = false;
	m_hasBeenSightedIn = false;

	if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
	{
		NDebugOverlay::Cross3D( lookAtPos, 2.0f, 255, 255, 100, true, 2.0f * duration );
		
		char *priName = "";
		switch( priority )
		{
			case BORING:		priName = "BORING"; break;
			case INTERESTING:	priName = "INTERESTING"; break;
			case IMPORTANT:		priName = "IMPORTANT"; break;
			case CRITICAL:		priName = "CRITICAL"; break;		
		}
		
		ConColorMsg( Color( 255, 100, 0, 255 ), "%3.2f: %s Look At ( %g, %g, %g ) for %3.2f s, Pri = %s, Reason = %s\n",
						gpGlobals->curtime,
						m_player->GetPlayerName(),
						lookAtPos.x, lookAtPos.y, lookAtPos.z,
						duration,
						priName,
						( reason ) ? reason : "" );	
	}
}


//-----------------------------------------------------------------------------------------------
/**
 * Aim the bot's head towards the given goal
 */
void PlayerBody::AimHeadTowards( CBaseEntity *subject, LookAtPriorityType priority, float duration, INextBotReply *replyWhenAimed, const char *reason )
{
	if ( duration <= 0.0f )
	{
		duration = 0.1f;
	}

	if ( subject == NULL )
	{
		return;
	}

	// don't spaz our aim around
	if ( m_lookAtPriority == priority )
	{
		if ( !IsHeadSteady() || GetHeadSteadyDuration() < NextBotHeadAimSettleDuration.GetFloat() )
		{
			// we're still finishing a look-at at the same priority
			if ( replyWhenAimed ) 
			{
				replyWhenAimed->OnFail( GetBot(), INextBotReply::DENIED );
			}

			if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
			{
				ConColorMsg( Color( 255, 0, 0, 255 ), "%3.2f: %s Look At rejected - previous aim not settled\n",
								gpGlobals->curtime,
								m_player->GetPlayerName() );
			}
			return;
		}
	}

	// don't short-circuit if "sighted in" to avoid rapid view jitter
	if ( m_lookAtPriority > priority && !m_lookAtExpireTimer.IsElapsed() )
	{
		// higher priority lookat still ongoing
		if ( replyWhenAimed ) 
		{
			replyWhenAimed->OnFail( GetBot(), INextBotReply::DENIED );
		}

		if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
		{
			ConColorMsg( Color( 255, 0, 0, 255 ), "%3.2f: %s Look At rejected - higher priority aim in progress\n",
							gpGlobals->curtime,
							m_player->GetPlayerName() );
		}
		return;
	}

	if ( m_lookAtReplyWhenAimed )
	{
		// in-process aim was interrupted
		m_lookAtReplyWhenAimed->OnFail( GetBot(), INextBotReply::INTERRUPTED );
	}

	m_lookAtReplyWhenAimed = replyWhenAimed;
	m_lookAtExpireTimer.Start( duration );

	// if given the same subject, just update priority
	if ( subject == m_lookAtSubject )
	{
		m_lookAtPriority = priority;
		return;
	}

	// new subject
	m_lookAtSubject = subject;

#ifdef REFACTOR_FOR_CLIENT_SIDE_EYE_TRACKING
	CBasePlayer *pMyPlayer = static_cast< CBasePlayer * >( GetEntity() );
	if ( subject->IsPlayer() )
	{
		// looking at a player, look at their eye position
		TerrorPlayer *pMyTarget = ToTerrorPlayer( subject );
		m_lookAtPos = subject->EyePosition();
		if(pMyPlayer)
		{
			pMyPlayer->SetLookatPlayer( pMyTarget );
		}
	}
	else
	{
		// not looking at a player
		m_lookAtPos = subject->WorldSpaceCenter();
		if(pMyPlayer)
		{
			pMyPlayer->SetLookatPlayer( NULL );
		}
	}
#endif

	m_lookAtPriority = priority;
	m_lookAtDurationTimer.Start();
	m_isSightedIn = false;
	m_hasBeenSightedIn = false;

	if ( GetBot()->IsDebugging( NEXTBOT_LOOK_AT ) )
	{
		NDebugOverlay::Cross3D( m_lookAtPos, 2.0f, 100, 100, 100, true, duration );
		
		char *priName = "";
		switch( priority )
		{
			case BORING:		priName = "BORING"; break;
			case INTERESTING:	priName = "INTERESTING"; break;
			case IMPORTANT:		priName = "IMPORTANT"; break;
			case CRITICAL:		priName = "CRITICAL"; break;		
		}
		
		ConColorMsg( Color( 255, 100, 0, 255 ), "%3.2f: %s Look At subject %s for %3.2f s, Pri = %s, Reason = %s\n",
						gpGlobals->curtime,
						m_player->GetPlayerName(),
						subject->GetClassname(),
						duration,
						priName,
						( reason ) ? reason : "" );	
	}
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if head is not rapidly turning to look somewhere else
 */
bool PlayerBody::IsHeadSteady( void ) const
{
	return m_headSteadyTimer.HasStarted();
}


//-----------------------------------------------------------------------------------------------
/**
 * Return the duration that the bot's head has been on-target
 */
float PlayerBody::GetHeadSteadyDuration( void ) const
{
	// return ( IsHeadAimingOnTarget() ) ? m_headSteadyTimer.GetElapsedTime() : 0.0f;
	return m_headSteadyTimer.HasStarted() ? m_headSteadyTimer.GetElapsedTime() : 0.0f;
}


//-----------------------------------------------------------------------------------------------
float PlayerBody::GetMaxHeadAngularVelocity( void ) const
{
	return NextBotSaccadeSpeed.GetFloat();
}


//-----------------------------------------------------------------------------------------------
bool PlayerBody::StartActivity( Activity act, unsigned int flags )
{
	// player animation state is controlled on the client
	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return currently animating activity
 */
Activity PlayerBody::GetActivity( void ) const
{
	return ACT_INVALID;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if currently animating activity matches the given one
 */
bool PlayerBody::IsActivity( Activity act ) const
{
	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if currently animating activity has any of the given flags
 */
bool PlayerBody::HasActivityType( unsigned int flags ) const
{
	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Request a posture change
 */
void PlayerBody::SetDesiredPosture( PostureType posture )
{
	m_posture = posture;
}


//-----------------------------------------------------------------------------------------------
/**
 * Get posture body is trying to assume
 */
IBody::PostureType PlayerBody::GetDesiredPosture( void ) const
{
	return m_posture;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if body is trying to assume this posture
 */
bool PlayerBody::IsDesiredPosture( PostureType posture ) const
{
	return ( posture == m_posture );
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if body's actual posture matches its desired posture
 */
bool PlayerBody::IsInDesiredPosture( void ) const
{
	return true;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return body's current actual posture
 */
IBody::PostureType PlayerBody::GetActualPosture( void ) const
{
	return m_posture;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if body is actually in the given posture
 */
bool PlayerBody::IsActualPosture( PostureType posture ) const
{
	return ( posture == m_posture );
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if body's current posture allows it to move around the world
 */
bool PlayerBody::IsPostureMobile( void ) const
{
	return true;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if body's posture is in the process of changing to new posture
 */
bool PlayerBody::IsPostureChanging( void ) const
{
	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Arousal level change
 */
void PlayerBody::SetArousal( ArousalType arousal )
{
	m_arousal = arousal;
}


//-----------------------------------------------------------------------------------------------
/**
 * Get arousal level
 */
IBody::ArousalType PlayerBody::GetArousal( void ) const
{
	return m_arousal;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if body is at this arousal level
 */
bool PlayerBody::IsArousal( ArousalType arousal ) const
{
	return ( arousal == m_arousal );
}


//-----------------------------------------------------------------------------------------------
/**
 * Width of bot's collision hull in XY plane
 */
float PlayerBody::GetHullWidth( void ) const
{
	return VEC_HULL_MAX.x - VEC_HULL_MIN.x;
}


//-----------------------------------------------------------------------------------------------
/**
 * Height of bot's current collision hull based on posture
 */
float PlayerBody::GetHullHeight( void ) const
{
	if ( m_posture == CROUCH )
	{
		return GetCrouchHullHeight();
	}

	return GetStandHullHeight();
}


//-----------------------------------------------------------------------------------------------
/**
 * Height of bot's collision hull when standing
 */
float PlayerBody::GetStandHullHeight( void ) const
{
	return VEC_HULL_MAX.z - VEC_HULL_MIN.z;
}


//-----------------------------------------------------------------------------------------------
/**
 * Height of bot's collision hull when crouched
 */
float PlayerBody::GetCrouchHullHeight( void ) const
{
	return VEC_DUCK_HULL_MAX.z - VEC_DUCK_HULL_MIN.z;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return current collision hull minimums based on actual body posture
 */
const Vector &PlayerBody::GetHullMins( void ) const
{
	if ( m_posture == CROUCH )
	{
		m_hullMins = VEC_DUCK_HULL_MIN;
	}
	else
	{
		m_hullMins = VEC_HULL_MIN;
	}
	
	return m_hullMins;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return current collision hull maximums based on actual body posture
 */
const Vector &PlayerBody::GetHullMaxs( void ) const
{
	if ( m_posture == CROUCH )
	{
		m_hullMaxs = VEC_DUCK_HULL_MAX;
	}
	else
	{
		m_hullMaxs = VEC_HULL_MAX;
	}

	return m_hullMaxs;	
}


//-----------------------------------------------------------------------------------------------
/**
 * Return the bot's collision mask (hack until we get a general hull trace abstraction here or in the locomotion interface)
 */
unsigned int PlayerBody::GetSolidMask( void ) const
{
	return ( m_player ) ? m_player->PlayerSolidMask() : MASK_PLAYERSOLID;
}





