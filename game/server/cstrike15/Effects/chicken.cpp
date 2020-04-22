// chicken.cpp
// An interactive, shootable chicken

#include "cbase.h"
#include "chicken.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "particle_parse.h"
#include "engine/IEngineSound.h"
#include "cs_simple_hostage.h"
#include "cs_player_resource.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CChicken )
	DEFINE_ENTITYFUNC( ChickenTouch ),
	DEFINE_THINKFUNC( ChickenThink ),
	DEFINE_USEFUNC( ChickenUse ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CChicken, DT_CChicken )
SendPropBool( SENDINFO( m_jumpedThisFrame ) ),
SendPropEHandle( SENDINFO( m_leader ) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( chicken, CChicken );
PRECACHE_REGISTER( chicken );

class HostagePathCost;

extern ConVar hostage_debug;

#define CHICKEN_ZOMBIE_SPAWN_DURATION 3.5f

#define CHICKEN_DISTANCE_RUN 200.0f
#define CHICKEN_DISTANCE_WALK 100.0f

//-----------------------------------------------------------------------
CChicken::CChicken()
{
}


//-----------------------------------------------------------------------
CChicken::~CChicken()
{
}


//-----------------------------------------------------------------------
void CChicken::Precache( void )
{
	SetModelName( MAKE_STRING( "models/chicken/chicken.mdl" ) );	// prop precache wants this

	BaseClass::Precache();

	PrecacheModel( "models/chicken/chicken.mdl" );
	PrecacheModel( "models/chicken/chicken_zombie.mdl" );
	PrecacheModel( "models/chicken/chicken_gone.mdl" );
	PrecacheModel( "models/antlers/antlers.mdl" );

	PrecacheScriptSound( "Chicken.Idle" );
	PrecacheScriptSound( "Chicken.Panic" );
	PrecacheScriptSound( "Chicken.Fly" );
	PrecacheScriptSound( "Chicken.FlapWings" );
	PrecacheScriptSound( "Chicken.Death" );
	PrecacheScriptSound( "Chicken.ZombieRez" );

	PrecacheParticleSystem( "weapon_confetti_omni" );
	PrecacheParticleSystem( "chicken_rez" );
	PrecacheParticleSystem( "chicken_gone_crumble_halloween" );
	PrecacheParticleSystem( "chicken_gone_zombie" );

	PrecacheParticleSystem( "impact_helmet_headshot" );
}

//-----------------------------------------------------------------------
void CChicken::Spawn( void )
{

	SetModel( "models/chicken/chicken.mdl" );

	BaseClass::Spawn();

	SetNextThink( gpGlobals->curtime );
	SetThink( &CChicken::ChickenThink );

	SetTouch( &CChicken::ChickenTouch );

	SetUse( &CChicken::ChickenUse );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );

	const model_t *pModel = modelinfo->GetModel( GetModelIndex() );
	if ( pModel )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( pModel, mins, maxs );
		mins.z = 0.0f;
		SetCollisionBounds( mins, maxs );
	}

	SetGravity( 1.0 );

	SetHealth( 1 );
	SetMaxHealth( 1 );
	m_takedamage = DAMAGE_YES;

	Idle();
	m_fleeFrom = NULL;
	m_updateTimer.Invalidate();
	m_reuseTimer.Invalidate( );
	m_moveRateThrottleTimer.Invalidate( );

	m_stuckAnchor = GetAbsOrigin();
	m_stuckTimer.Start( 1.0f );

	m_isOnGround = false;

	m_startleTimer.Invalidate();
	ListenForGameEvent( "weapon_fire" );
	//ListenForGameEvent( "bullet_impact" );

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
	{
		SetBodygroup( 1, 1 ); // birthday hat
	}

	m_leader = INVALID_EHANDLE;
	m_reuseTimer.Invalidate( );
	m_hasBeenUsed = false;

	m_jumpedThisFrame = false;

	m_path.Invalidate( );
	m_repathTimer.Invalidate( );

	m_pathFollower.Reset( );
	m_pathFollower.SetPath( &m_path );
	m_pathFollower.SetImprov( this );

	m_lastKnownArea = NULL;

	// Need to make sure the hostages are on the ground when they spawn
//	Vector GroundPos = DropToGround( this, GetAbsOrigin( ), HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
//	SetAbsOrigin( GroundPos );

	m_bInJump = false;
	m_flLastJumpTime = 0;
	m_inhibitObstacleAvoidanceTimer.Invalidate( );

	m_isWaitingForLeader = false;

	m_lastLeaderID = 0;

	m_flActiveFollowStartTime = 0;

}


//-----------------------------------------------------------------------
void CChicken::ChickenTouch( CBaseEntity *pOther )
{
	if ( !CSGameRules() || CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
	{
		Flee( pOther, RandomFloat( 2.0f, 3.0f ) );
	}
}


bool CChicken::IsFollowingSomeone( void )
{
	return ( m_leader.m_Value != NULL );
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsFollowing( const CBaseEntity *entity )
{
	return ( m_leader.m_Value == entity );
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsOnGround( void ) const
{
	return ( GetFlags( ) & FL_ONGROUND );
}


//-----------------------------------------------------------------------------------------------------
/**
* Begin following "leader"
*/
void CChicken::Follow( CCSPlayer *leader )
{
	m_lastLeaderID = 0;

	if ( leader )
	{
		leader->IncrementNumFollowers( );
		m_lastLeaderID = leader->GetUserID( );
		m_leader = leader;
	}
	else
	{
		m_leader = INVALID_EHANDLE;
	}

	m_leader = leader;
	m_isWaitingForLeader = false;

	m_flActiveFollowStartTime = gpGlobals->curtime;

	m_moveRateThrottleTimer.Start( 1.0f);
}

//-----------------------------------------------------------------------------------------------------
/**
* Return our leader, or NULL
*/
CCSPlayer *CChicken::GetLeader( void ) const
{
	return ToCSPlayer( m_leader.m_Value );
}



//-----------------------------------------------------------------------------------------------------
/**
* Invoked when a chicken is "used" by a player
*/
void CChicken::ChickenUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{

	CCSPlayer *pPlayer = ToCSPlayer( pActivator );
	if ( !pPlayer )
		return;


	// limit use range
	float useRange = 1000.0f;
	Vector to = pActivator->GetAbsOrigin( ) - GetAbsOrigin( );
	if ( to.IsLengthGreaterThan( useRange ) )
	{
		return;
	}

	SetChickenStartFollowingPlayer( pPlayer );
	
	
}

void CChicken::SetChickenStartFollowingPlayer( CCSPlayer *pPlayer )
{

	// throttle how often leader can change
	if ( !m_reuseTimer.IsElapsed( ) )
	{
		return;
	}

	// if we are already following the player who used us, stop following
	if ( IsFollowing( pPlayer ) )
	{
		Follow( NULL );
		Idle( );
		EmitSound( "Chicken.Idle" );
	}
	else
	{
		// if we're already following a CT, ignore new uses
		if ( IsFollowingSomeone( ) )
		{
			return;
		}

		// start following
		Follow( pPlayer );
		EmitSound( "Chicken.FlapWings" );
		Jump( 50.0f );
	}

	m_reuseTimer.Start( 1.0f );
}

//--------------------------------------------------------------------------------------------------------------
/**
* Rotate body to face towards "target"
*/
void CChicken::FaceTowards( const Vector &target, float deltaT )
{
	Vector to = target - GetFeet( );
	to.z = 0.0f;

	QAngle desiredAngles;
	VectorAngles( to, desiredAngles );

	QAngle angles = GetAbsAngles( );

	// The animstate system for hostages will smooth out the transition to this direction, so no need to double smooth it here.
	angles.y = desiredAngles.y;

	SetAbsAngles( angles );
}

int CChicken::OnTakeDamage( const CTakeDamageInfo &info )
{
	/*
	if ( EconHolidays_IsHolidayActive( kHoliday_Halloween ) )
	{
		// if we recently turned into a zombie, ignore damage for CHICKEN_ZOMBIE_SPAWN_DURATION seconds.
		if ( IsZombie() )
		{
			if ( (gpGlobals->curtime - m_flWhenZombified) > CHICKEN_ZOMBIE_SPAWN_DURATION )
			{
				return BaseClass::OnTakeDamage( info );
			}
			else
			{
				return 0;
			}
		}
		else
		{
			if ( m_activity != ACT_GLIDE && m_isOnGround )
			{
				//when there's no more room in chicken-hell... turn into a chicken zombie!
				Zombify();

				SetModel( "models/chicken/chicken_zombie.mdl" );
				m_activity = ACT_CLIMB_UP;
				ResetSequence( LookupSequence( "spawn" ) );
				ResetSequenceInfo();
				ForceCycle( 0 );

				DispatchParticleEffect( "chicken_gone_crumble_halloween", GetAbsOrigin() + Vector( 0, 0, 8 ), QAngle( 0, 0, 0 ) );

				CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
				trace_t tr;
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, -16 ), MASK_PLAYERSOLID, &traceFilter, &tr );

				Vector	vecPos, vecVel, vecNormal;
				QAngle angNormal;

				vecNormal = tr.DidHit() ? tr.plane.normal : Vector( 0, 0, 0 );
				//vecNormal.y *= -1;
				//vecNormal.z *= -1;
				VectorAngles( vecNormal, angNormal );
				angNormal.x += 90;

				DispatchParticleEffect( "chicken_rez", GetAbsOrigin(), angNormal );

				CSoundParameters params;
				if ( GetParametersForSound( "Chicken.ZombieRez", params, NULL ) )
				{
					EmitSound_t ep( params );
					ep.m_pOrigin = &GetAbsOrigin();

					CBroadcastRecipientFilter filter;
					EmitSound( filter, SOUND_FROM_WORLD, ep );
				}

				SetSolid( SOLID_NONE );

				return 0;
			}
			else
			{
				return BaseClass::OnTakeDamage( info );
			}
		}
	}
	else
	*/
	{
		return BaseClass::OnTakeDamage( info );
	}
}


//-----------------------------------------------------------------------
void CChicken::Event_Killed( const CTakeDamageInfo &info )
{
	EmitSound( "Chicken.Death" );

	if ( CSGameRules() && CSGameRules()->IsCSGOBirthday() )
		DispatchParticleEffect( "weapon_confetti_omni", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

	if ( IsZombie() )
		DispatchParticleEffect( "chicken_gone_zombie", GetAbsOrigin() + Vector( 0, 0, 8 ), QAngle( 0, 0, 0 ) );

	if ( IsFollowingSomeone( ) )
	{
		CSingleUserRecipientFilter leaderfilter( ToCSPlayer( m_leader.m_Value ) );
		leaderfilter.MakeReliable( );

		float flFollowDuration = gpGlobals->curtime - m_flActiveFollowStartTime;

		UTIL_ClientPrintFilter( leaderfilter, HUD_PRINTTALK, "#Pet_Killed", CFmtStr( "%.0f", flFollowDuration ).Get() );
	}

	BaseClass::Event_Killed( info );
}


//-----------------------------------------------------------------------
void CChicken::FireGameEvent( IGameEvent *event )
{
	// all the events we care about scare us

	if ( m_startleTimer.HasStarted() )
	{
		// we're already scared
		return;
	}

	if ( event->GetBool( "silenced" ) )
	{
		// Silenced weapons don't scare chickens
		return;
	}

	CBasePlayer *player = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( player )
	{
		const float fleeGunfireRange = 1000.0f;

		Vector toPlayer = player->GetAbsOrigin() - GetAbsOrigin();

		if ( toPlayer.IsLengthLessThan( fleeGunfireRange ) )
		{
			m_startleTimer.Start( RandomFloat( 0.1f, 0.5f ) );
			m_fleeFrom = player;
		}
	}
}


//-----------------------------------------------------------------------
void CChicken::Idle()
{
//	m_leader = NULL;

	m_activity = ACT_IDLE;
	m_activityTimer.Start( RandomFloat( 0.5, 3.0f ) );

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();

	m_pathFollower.ResetStuck( );
}


//-----------------------------------------------------------------------
void CChicken::Walk()
{

	m_activity = ACT_WALK;
	m_activityTimer.Start( RandomFloat( 0.5, 3.0f ) );
	m_turnRate = RandomFloat( -45.0f, 45.0f );

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();
}




//-----------------------------------------------------------------------
void CChicken::Flee( CBaseEntity *fleeFrom, float duration )
{
	if ( m_activity != ACT_RUN || m_vocalizeTimer.IsElapsed() )
	{
		// throttle interval between vocalizations
		m_vocalizeTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		EmitSound( "Chicken.Panic" );
	}

	m_activity = ACT_RUN;
	m_activityTimer.Start( duration );
	m_turnRate = 0.0f;
	m_fleeFrom = fleeFrom;

	SetSequence( SelectWeightedSequence( m_activity ) );
	
	ResetSequenceInfo();
}


//-----------------------------------------------------------------------
void CChicken::Fly()
{
	if ( m_activity != ACT_GLIDE || m_vocalizeTimer.IsElapsed() )
	{
		// throttle interval between vocalizations
		m_vocalizeTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		EmitSound( "Chicken.Fly" );
	}

	m_activity = ACT_GLIDE;
	m_turnRate = 0.0f;

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();
}


//-----------------------------------------------------------------------
void CChicken::Land()
{
	if ( m_activity != ACT_LAND || m_vocalizeTimer.IsElapsed() )
	{
		// throttle interval between vocalizations
		m_vocalizeTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		EmitSound( "Chicken.Idle" );
	}

	m_activity = ACT_LAND;
	m_turnRate = 0.0f;

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo();
}


//-----------------------------------------------------------------------
void CChicken::Update( void )
{
	if ( GetLeader( ) )
		return;

	if ( !m_updateTimer.IsElapsed() )
		return;

	m_updateTimer.Start( RandomFloat( 0.5f, 1.0f ) );

	// find closest visible player
	CUtlVector< CBasePlayer * > playerVector;
	CollectPlayers( &playerVector, TEAM_ANY, COLLECT_ONLY_LIVING_PLAYERS );

	float closeRangeSq = FLT_MAX;
	CBasePlayer *close = NULL;

	for( int i=0; i<playerVector.Count(); ++i )
	{
		Vector toPlayer = playerVector[i]->GetAbsOrigin() - GetAbsOrigin();
		float rangeSq = toPlayer.LengthSqr();

		// Only scare chickens if we're close
		if ( rangeSq < closeRangeSq )
		{
			// Only scare chickens if we're moving fast enough to make noise
			Vector vPlayerVelocity;
			playerVector[i]->GetVelocity( &vPlayerVelocity, NULL );

			if ( vPlayerVelocity.Length() > 126.0f )
			{
				if ( playerVector[i]->IsLineOfSightClear( this ) )
				{
					closeRangeSq = rangeSq;
					close = playerVector[i];
				}
			}
		}
	}

	const float tooClose = 200.0f;

	if ( close && closeRangeSq < tooClose * tooClose )
	{
		Flee( close, RandomFloat( 0.5, 1.0f ) );
	}
}


//-----------------------------------------------------------------------
float CChicken::AvoidObstacles( void )
{
	const float feelerRange = 15.0f;

	const Vector &forward = Forward();

	Vector left( -forward.y, forward.x, 0.0f );
	Vector right( forward.y, -forward.x, 0.0f );

	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	trace_t resultLeft;
	UTIL_TraceLine( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + left ), MASK_PLAYERSOLID, &traceFilter, &resultLeft );
	//NDebugOverlay::Line( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + left ), 0, 0, 255, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );

	trace_t resultRight;
	UTIL_TraceLine( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + right ), MASK_PLAYERSOLID, &traceFilter, &resultRight );
	//NDebugOverlay::Line( WorldSpaceCenter(), WorldSpaceCenter() + feelerRange * ( forward + right ), 255, 0, 0, true, NDEBUG_PERSIST_TILL_NEXT_SERVER );

	const float maxTurnRate = 360.0f;
	float turnRate = 0.0f;

	if ( resultLeft.DidHit() )
	{
		if ( resultRight.DidHit() )
		{
			// both sides hit
			if ( resultLeft.fraction < resultRight.fraction )
			{
				// left hit closer - turn right
				turnRate = -maxTurnRate;
			}
			else
			{
				// right hit closer - turn left
				turnRate = maxTurnRate;
			}
		}
		else
		{
			// left hit - turn right
			turnRate = -maxTurnRate;
		}
	}
	else if ( resultRight.DidHit() )
	{
		// right hit - turn left
		turnRate = maxTurnRate;
	}

	Vector toAnchor = m_stuckAnchor - GetAbsOrigin();

	if ( toAnchor.IsLengthGreaterThan( 50.0f ) )
	{
		m_stuckAnchor = GetAbsOrigin();
		m_stuckTimer.Reset();
	}

	return turnRate;
}


//-----------------------------------------------------------------------
void CChicken::ResolveCollisions( const Vector &desiredPosition, float deltaT )
{
	CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
	trace_t result;

	const float stepHeight = WorldAlignSize().z / 2.0f;

	// try to do full move
	Vector testPosition = desiredPosition;

	for( int slideCount=0; slideCount<3; ++slideCount )
	{
		UTIL_TraceHull( GetAbsOrigin(), testPosition, WorldAlignMins() + Vector( 0, 0, stepHeight ), WorldAlignMaxs(), MASK_PLAYERSOLID, &traceFilter, &result );

		if ( !result.DidHit() )
		{
			break;
		}

		Vector fullMove = testPosition - GetAbsOrigin();
		float blocked = DotProduct( fullMove, result.plane.normal );

		testPosition = GetAbsOrigin() + fullMove - blocked * result.plane.normal;
	}

	Vector resolvedPosition = result.endpos;

	// snap to ground (or fall)
	UTIL_TraceHull( resolvedPosition + Vector( 0, 0, stepHeight ), resolvedPosition + Vector( 0, 0, -stepHeight ), WorldAlignMins(), WorldAlignMaxs(), MASK_PLAYERSOLID, &traceFilter, &result );

	if ( result.DidHit( ) && !IsJumping( ) )
	{
		// limit slope that can be walked up
		const float slopeLimit = 0.7071f;
		if ( !result.plane.normal.IsZero() && result.plane.normal.z > slopeLimit )
		{
			SetAbsOrigin( result.endpos );
		}
		else
		{
			SetAbsOrigin( resolvedPosition );
		}

		if ( !m_isOnGround )
		{
			Land();

			m_isOnGround = true;
			m_bInJump = false;

		}
	}
	else
	{
		SetAbsOrigin( resolvedPosition );

		// fall
		SetBaseVelocity( GetBaseVelocity() + Vector( 0, 0, GetGravity() * deltaT ) );

		m_isOnGround = false;

		Fly();
	}
}


void CChicken::UpdateFollowing( float deltaT )
{
	if ( !IsFollowingSomeone( ) && m_lastLeaderID != 0 )
	{
		m_lastLeaderID = 0;
	}

	// if we have a leader, follow him
	CCSPlayer *leader = GetLeader( );
	if ( leader )
	{
		// if leader is dead, stop following him
		if ( !leader->IsAlive( ) )
		{
			Follow( NULL );
			Idle( );

			return;
		}

//		m_nHostageState = k_EHostageStates_FollowingPlayer;

		// if leader has moved, repath
		if ( m_path.IsValid( ) )
		{
			Vector pathError = leader->GetAbsOrigin( ) - m_path.GetEndpoint( );

			const float repathRange = 100.0f;
			if ( pathError.IsLengthGreaterThan( repathRange ) )
			{
				m_path.Invalidate( );
			}

//			m_activity = ACT_WALK;
		}

		// build a path to our leader
		if ( !m_path.IsValid( ) && m_repathTimer.IsElapsed( ) )
		{
			const float repathInterval = 0.5f;
			m_repathTimer.Start( repathInterval );

			Vector from = GetAbsOrigin( );
			Vector to = leader->GetAbsOrigin( );
			HostagePathCost pathCost;

			m_path.Compute( from, to, pathCost );
			m_pathFollower.Reset( );
		}


		// if our rescuer is too far away, give up
		const float giveUpRange = 2000.0f;
		const float maxPathLength = 4000.0f;
		Vector toLeader = leader->GetAbsOrigin( ) - GetAbsOrigin( );
		if ( toLeader.IsLengthGreaterThan( giveUpRange ) || ( m_path.IsValid( ) && m_path.GetLength( ) > maxPathLength ) )
		{
			if ( hostage_debug.GetInt( ) < 2 )
			{
				Idle( );
			}
			return;
		}


		// don't crowd the leader
		if ( m_isWaitingForLeader )
		{
			// we are close to our leader and waiting for him to move
			const float waitRange = 200.0f;
			if ( toLeader.IsLengthGreaterThan( waitRange ) )
			{
				// leader has moved away - follow him
				m_isWaitingForLeader = false;
			}
		}


		if ( !m_isWaitingForLeader )
		{
			// move along path towards the leader
			m_pathFollower.Update( deltaT, m_inhibitObstacleAvoidanceTimer.IsElapsed( ) );

			if ( hostage_debug.GetBool( ) )
			{
				m_pathFollower.Debug( true );
			}

			float flDist = GetFeet( ).DistTo( ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ) );
			const float minStuckJumpTime = 1.0f;
			if ( ( m_pathFollower.GetStuckDuration( ) > minStuckJumpTime ) && ( flDist > CHICKEN_DISTANCE_RUN * 2 ) )
			{
				Jump( );
			}

			if ( hostage_debug.GetBool( ) )
			{
				m_path.Draw( );
			}
		}
	}
}

//-----------------------------------------------------------------------
void CChicken::ChickenThink( void )
{

	float deltaT = gpGlobals->frametime;

	SetNextThink( gpGlobals->curtime + deltaT );

	// if we've been a zombie for less than CHICKEN_ZOMBIE_SPAWN_DURATION seconds
	float flZombieDuration = (gpGlobals->curtime - m_flWhenZombified);
	if ( IsZombie() && flZombieDuration < CHICKEN_ZOMBIE_SPAWN_DURATION )
	{

		m_activity = ACT_CLIMB_UP;
		SetSequence( LookupSequence( "spawn" ) );
		SetCycle( flZombieDuration / CHICKEN_ZOMBIE_SPAWN_DURATION );
		m_activityTimer.Start( 0 );
		SetSolid( SOLID_NONE );
	}
	else if ( IsZombie() && (m_flWhenZombified + 1.0f) > (gpGlobals->curtime - CHICKEN_ZOMBIE_SPAWN_DURATION) )
	{
		SetSolid( SOLID_BBOX );
		m_activity = ACT_WALK;
	}

	// do leader-following behavior, if necessary
	UpdateFollowing( deltaT );

	Update();

	if ( m_startleTimer.HasStarted() && m_startleTimer.IsElapsed() )
	{
		// we were startled by something and have just noticed it
		m_startleTimer.Invalidate();

		Flee( m_fleeFrom, RandomFloat( 2.0f, 3.0f ) );
	}

	if ( IsActivityFinished() && !IsFollowingSomeone() )
	{

		switch ( ( int ) m_activity )
		{
		case ACT_IDLE:
			if ( RandomInt( 1, 100 ) < 30 )
			{
				Walk( );
			}
			break;

		case ACT_WALK:
			if ( m_activityTimer.IsElapsed( ) )
			{
				Idle( );
			}
			break;

		case ACT_RUN:
			if ( m_activityTimer.IsElapsed( ) )
			{
				Walk( );
			}
			break;

		case ACT_GLIDE:
			Fly( );
			break;

		case ACT_LAND:
			Walk( );
			break;
		}
	}
	else// ongoing activity
	{
		if ( IsFollowingSomeone( ) )
		{
			if ( m_moveRateThrottleTimer.IsElapsed( ) )
			{
				float flDist = GetFeet( ).DistTo( ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ) );

				if ( flDist < ( CHICKEN_DISTANCE_WALK * 0.9f ) )
				{
					if ( m_activity != ACT_IDLE )
						Idle( );
				}
				else if ( flDist > ( CHICKEN_DISTANCE_WALK * 1.1f ) && flDist < ( CHICKEN_DISTANCE_RUN * 0.9f ) )
				{
					if ( m_activity != ACT_WALK )
						Walk( );
				}
				if ( flDist > ( CHICKEN_DISTANCE_RUN * 1.1f ) )
				{
					if ( m_activity != ACT_RUN )
						Run( );
				}

				m_moveRateThrottleTimer.Start( RandomFloat( 0.0f, 0.5f ) );
			}

		}

		if ( m_activity == ACT_IDLE || m_activity == ACT_WALK )
		{
			// sound like a calm chicken
			if ( m_vocalizeTimer.IsElapsed() )
			{
				m_vocalizeTimer.Start( RandomFloat( 1.5f, 4.5f ) );

				EmitSound( "Chicken.Idle" );
			}
		}


		// don't walk/run in a straight line - turn a bit
		switch ( ( int ) m_activity )
		{
		case ACT_WALK:
		case ACT_RUN:
			QAngle angle = GetAbsAngles( );

			if ( m_fleeFrom != NULL )
			{
				Vector fleeVector = GetAbsOrigin( ) - m_fleeFrom->GetAbsOrigin( );
				fleeVector.z = 0.0f;

				if ( IsZombie( ) )
					fleeVector = -fleeVector; //zombie chickens flee TOWARDS players

				float fleeRange = fleeVector.NormalizeInPlace( );

				const float safeRange = 150.0f;
				if ( fleeRange > safeRange )
				{
					m_fleeFrom = NULL;
				}
				else
				{
					m_activityTimer.Reset( );

					if ( DotProduct( fleeVector, Forward( ) ) < 0.7071f )
					{
						// turn away
						m_turnRate = 360.0f;

						if ( CrossProduct( fleeVector, Forward( ) ).z > 0.0f )
						{
							m_turnRate = -m_turnRate;
						}
					}
					else
					{
						m_turnRate = 0.0f;
					}
				}

				float obstacleTurnRate = AvoidObstacles( );

				float actualTurnRate = ( obstacleTurnRate == 0.0f ) ? m_turnRate : obstacleTurnRate;

				angle.y += actualTurnRate * deltaT;

				SetAbsAngles( angle );

			}
			else if ( IsFollowingSomeone( ) )
			{

				Vector followVector = {0,0,0};

				m_activityTimer.Reset( );

				CTraceFilterNoNPCsOrPlayer traceFilter( this, COLLISION_GROUP_NONE );
				trace_t tr;
				UTIL_TraceLine( GetEyes( ), ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ), MASK_PLAYERSOLID, &traceFilter, &tr );

				if ( tr.fraction == 1.0 )	// chicken can see player so target player
				{
					followVector = ToCSPlayer( m_leader.m_Value )->GetAbsOrigin( ) - GetAbsOrigin( );
				}
				else// chicken cannot see player so target next track node
				{
					followVector = m_vecPathGoal - GetAbsOrigin( );
				}

				float followRange = followVector.NormalizeInPlace( );

				// we use an softer angle threshold at greater distances. This makes the chicken seem more organic.
				float flAngleTheshold = RemapValClamped( followRange, 300, 2000, 0.96, 0.707 );

				if ( DotProduct( followVector, Forward( ) ) < flAngleTheshold )
				{
					m_turnRate = 360.0f;

					if ( CrossProduct( followVector, Forward( ) ).z > 0.0f )
					{
						m_turnRate = -m_turnRate;
					}
				}
				else
				{
					m_turnRate = 0.0f;
				}

				// If the path goal is above us then don't try to avoid obstacles even if one is present.
				// Instead brute force into a wiggle aka jump;
				//

				float actualTurnRate = m_turnRate;
				float obstacleTurnRate = AvoidObstacles( );

				if ( DotProduct( followVector, Up( ) ) >= .9f ) // the path goal is overhead
				{
					Jump( );
				}
				
				if ( obstacleTurnRate != 0.0f ) 
				{
					actualTurnRate = obstacleTurnRate;
				}

				angle.y += actualTurnRate * deltaT;

				SetAbsAngles( angle );;
			}

			break;
		}
	}

	Vector desiredPosition;


	Vector velocityFromAnimation = GetGroundSpeedVelocity();
	desiredPosition = GetAbsOrigin() + velocityFromAnimation * deltaT;

	ResolveCollisions( desiredPosition, deltaT );

	SetPlaybackRate( 1.0f );
	StudioFrameAdvance();
}


const Vector &CChicken::GetCentroid( void ) const
{
	static Vector centroid;

	centroid = GetFeet( );
	centroid.z += HalfHumanHeight;

	return centroid;
}

//-----------------------------------------------------------------------------------------------------
/**
* Return position of "feet" - point below centroid of improv at feet level
*/
const Vector &CChicken::GetFeet( void ) const
{
	static Vector feet;

	feet = GetAbsOrigin( );

	return feet;
}

//-----------------------------------------------------------------------------------------------------
const Vector &CChicken::GetEyes( void ) const
{
	static Vector eyes;

	eyes = EyePosition( );

	return eyes;
}

//-----------------------------------------------------------------------------------------------------
/**
* Return direction of movement
*/
float CChicken::GetMoveAngle( void ) const
{
	return GetAbsAngles( ).y;
}

//-----------------------------------------------------------------------------------------------------
CNavArea *CChicken::GetLastKnownArea( void ) const
{
	return m_lastKnownArea;
}

//-----------------------------------------------------------------------------------------------------
/**
* Find "simple" ground height, treating current nav area as part of the floo
*/
bool CChicken::GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal )
{
	if ( TheNavMesh->GetSimpleGroundHeight( pos, height, normal ) )
	{
		// our current nav area also serves as a ground polygon
		if ( m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ) )
		{
			*height = MAX( ( *height ), m_lastKnownArea->GetZ( pos ) );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
void CChicken::Crouch( void )
{
// 	m_isCrouching = true;
}

//-----------------------------------------------------------------------------------------------------
/**
* un-crouch
*/
void CChicken::StandUp( void )
{
// 	m_isCrouching = false;
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsCrouching( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
/**
* Initiate a jump
*/
void CChicken::Jump( void )
{
	float flJumpPower = 250.0f; /*RandomFloat( 10.0f, 200.0f );*/ /*RemapValClamped( flTimeSinceLastJump, 3.0, 20.0, 200.0f, 30.0f );*/

	Jump( flJumpPower );
}


void CChicken::Jump( float flVelocity )
{
	// don't jump if the nav disallows it
	CNavArea *myArea = GetLastKnownArea( );
	if ( myArea && myArea->HasAttributes( NAV_MESH_NO_JUMP ) )
		return;

	if ( CanJump( ) && IsOnGround( ) )
	{
		const float minJumpInterval = 0.1f;
		m_jumpTimer.Start( minJumpInterval );

		m_bInJump = true;

		// 
		// 		Vector dir;
		// 		AngleVectors( GetAbsAngles( ), &dir, NULL, NULL );
		// 
		// 		vel += dir * 10;

		Vector vel = GetAbsVelocity( );
		vel.z += flVelocity;
		SetAbsVelocity( vel );


		SetSequence( SelectWeightedSequence( ACT_RUN ) );
		ResetSequenceInfo( );

		m_jumpedThisFrame = true;

		m_flLastJumpTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsJumping( void ) const
{
	//return m_bInJump;

	return !m_jumpTimer.IsElapsed( );
}

bool CChicken::CanJump( void ) const
{
	return !IsJumping( ) && ( gpGlobals->curtime - m_flLastJumpTime > 3.0f );
}

//-----------------------------------------------------------------------------------------------------
/**
* Set movement speed to running
*/
void CChicken::Run( void )
{
//	m_isRunning = true;

	m_activity = ACT_RUN;
	m_activityTimer.Start( RandomFloat( 0.5, 3.0f ) );
	m_turnRate = RandomFloat( -45.0f, 45.0f );

	SetSequence( SelectWeightedSequence( m_activity ) );
	ResetSequenceInfo( );

}


// -----------------------------------------------------------------------------------------------------
// /**
// * Set movement speed to walking
// */
// void CChicken::Walk( void )
// {
// 	m_isRunning = false;
// }


//-----------------------------------------------------------------------------------------------------
bool CChicken::IsRunning( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when a ladder is encountered while following a path
*/
void CChicken::StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos )
{
}

//-----------------------------------------------------------------------------------------------------
/**
* Traverse given ladder
*/
bool CChicken::TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT )
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
bool CChicken::IsUsingLadder( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
void CChicken::TrackPath( const Vector &pathGoal, float deltaT )
{
	m_vecPathGoal = pathGoal;
	
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when an improv reaches its MoveTo goal
*/
void CChicken::OnMoveToSuccess( const Vector &goal )
{
}

//-----------------------------------------------------------------------------------------------------
/**
* Invoked when an improv fails to reach a MoveTo goal
*/
void CChicken::OnMoveToFailure( const Vector &goal, MoveToFailureType reason )
{
}

