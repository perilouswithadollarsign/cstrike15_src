/**
 * Inferno.cpp
 * An Inferno
 * Author: Michael S. Booth, February 2005
 * Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved
 */

#include "cbase.h"
#include "inferno.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include <coordsize.h>
#include "tier0/vprof.h"
#include "igameevents.h"
#include "particle_parse.h"
#include "entityutil.h"
#include "func_elevator.h"
#include "nav.h"
#include "nav_mesh.h"
#include "cs_shareddefs.h"
#include "smokegrenade_projectile.h"
#include "improv_locomotor.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_SERVERCLASS_ST( CInferno, DT_Inferno )
	SendPropArray3( SENDINFO_ARRAY3(m_fireXDelta), SendPropInt( SENDINFO_ARRAY(m_fireXDelta), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_fireYDelta), SendPropInt( SENDINFO_ARRAY(m_fireYDelta), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_fireZDelta), SendPropInt( SENDINFO_ARRAY(m_fireZDelta), COORD_INTEGER_BITS+1, 0 ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_bFireIsBurning), SendPropBool( SENDINFO_ARRAY(m_bFireIsBurning) ) ),	
	//SendPropArray3( SENDINFO_ARRAY3(m_BurnNormal), SendPropVector( SENDINFO_NOCHECK( m_BurnNormal ), 0, SPROP_NORMAL ) ),
	SendPropInt( SENDINFO(m_fireCount), 7, SPROP_UNSIGNED ),
END_SEND_TABLE()


BEGIN_DATADESC( CInferno )
	DEFINE_THINKFUNC( InfernoThink ),
END_DATADESC()


LINK_ENTITY_TO_CLASS( inferno, CInferno );
PRECACHE_REGISTER( inferno );


IMPLEMENT_SERVERCLASS_ST( CFireCrackerBlast, DT_FireCrackerBlast )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( fire_cracker_blast, CFireCrackerBlast );
PRECACHE_REGISTER( fire_cracker_blast );

ConVar InfernoPerFlameSpawnDuration( "inferno_per_flame_spawn_duration", "3", FCVAR_CHEAT, "Duration each new flame will attempt to spawn new flames" );
ConVar InfernoInitialSpawnInterval( "inferno_initial_spawn_interval", "0.02", FCVAR_CHEAT, "Time between spawning flames for first fire" );
ConVar InfernoChildSpawnIntervalMultiplier( "inferno_child_spawn_interval_multiplier", "0.1", FCVAR_CHEAT, "Amount spawn interval increases for each child" );
ConVar InfernoMaxChildSpawnInterval( "inferno_max_child_spawn_interval", "0.5", FCVAR_CHEAT, "Largest time interval for child flame spawning" );
ConVar InfernoSpawnAngle( "inferno_spawn_angle", "45", FCVAR_CHEAT, "Angular change from parent" );
ConVar InfernoMaxFlames( "inferno_max_flames", "16", FCVAR_CHEAT, "Maximum number of flames that can be created" );
ConVar InfernoFlameSpacing( "inferno_flame_spacing", "42", FCVAR_CHEAT, "Minimum distance between separate flame spawns" );
ConVar InfernoFlameLifetime( "inferno_flame_lifetime", "7", FCVAR_CHEAT, "Average lifetime of each flame in seconds" );
ConVar InfernoFriendlyFireDuration( "inferno_friendly_fire_duration", "6", FCVAR_CHEAT, "For this long, FF is credited back to the thrower." );
ConVar InfernoDebug( "inferno_debug", "0", FCVAR_CHEAT );
ConVar InfernoDamage( "inferno_damage", "40", FCVAR_CHEAT, "Damage per second" );
ConVar InfernoMaxRange( "inferno_max_range", "150", FCVAR_CHEAT, "Maximum distance flames can spread from their initial ignition point" );
ConVar InfernoVelocityFactor( "inferno_velocity_factor", "0.003", FCVAR_CHEAT );
ConVar InfernoVelocityDecayFactor( "inferno_velocity_decay_factor", "0.2", FCVAR_CHEAT );
ConVar InfernoVelocityNormalFactor( "inferno_velocity_normal_factor", "0", FCVAR_CHEAT );
ConVar InfernoSurfaceOffset( "inferno_surface_offset", "20", FCVAR_CHEAT );
ConVar InfernoChildSpawnMaxDepth( "inferno_child_spawn_max_depth", "4", FCVAR_CHEAT );
ConVar inferno_scorch_decals( "inferno_scorch_decals", "1", FCVAR_CHEAT );
ConVar inferno_max_trace_per_tick("inferno_max_trace_per_tick", "16");
ConVar inferno_forward_reduction_factor( "inferno_forward_reduction_factor", "0.9", FCVAR_CHEAT );

// Inferno trace masks can allow to do different traces for spreading fire
#define INFERNO_MASK_TO_GROUND ((MASK_SOLID_BRUSHONLY) & (~CONTENTS_GRATE))
#define INFERNO_MASK_LOS_CHECK ( INFERNO_MASK_TO_GROUND | CONTENTS_MONSTER )
#define INFERNO_MASK_DAMAGE INFERNO_MASK_LOS_CHECK

// Smoke grenade radius constant is actually tuned for the bots
// and not for gameplay. Visualizing smoke will show that it goes
// up from the emitter by 128 units (fuzzy top), nothing goes down,
// and it makes a wide XY-donut with a radius of *128* units (fuzzy edges).
ASSERT_INVARIANT( CONSTANT_UNITS_SMOKEGRENADERADIUS == 166 );
// When interacting with fire we don't want any vertical interactions unless
// contact points are definitely in smoke vertically.
static const float SmokeGrenadeRadius_InfernoAffectingZ = 120.0f;
// When interacting with fire on the same plane we don't want alpha depth-fighting
// in the most common case, so leave a grace margin between the smoke particles
// and the fire particles.
static const float SmokeGrenadeRadius_InfernoAffectingXY_topedge = 100.0f;
static const float SmokeGrenadeRadius_InfernoAffectingXY_equator = 150.0f;
static const float SmokeGrenadeRadius_InfernoAffectingXY_bottomedge = 128.0f;

// Fire burning things and smoke constants
static const float InfernoFire_HalfWidth = 30.0f;
static const float InfernoFire_FullHeight = 80.0f;

static bool BCheckFirePointInSmokeCloud( const Vector &vecFirePoint, const Vector &vecSmokeOrigin )
{
	const float flFireUpToSmokeCheckHeight = ( 2 * InfernoFire_HalfWidth + 4.0f );
	vec_t flFireAboveSmokeZ = ( vecFirePoint.z - vecSmokeOrigin.z );
	if ( flFireAboveSmokeZ < -flFireUpToSmokeCheckHeight )
		return false;	// fire not tall enough to burn up to smoke
	if ( flFireAboveSmokeZ > SmokeGrenadeRadius_InfernoAffectingZ )
		return false;	// smoke cloud not tall enough to reach to the fire
	
	// Now we know that fire is in XY-slice containing the smoke cloud
	// Figure out if we are in the equator XY-plane or in the shrinking edge XY-plane
	float flRadiusSquaredTest = SmokeGrenadeRadius_InfernoAffectingXY_equator*SmokeGrenadeRadius_InfernoAffectingXY_equator;
	if ( flFireAboveSmokeZ > SmokeGrenadeRadius_InfernoAffectingZ * 0.6f )
	{
		float flPctFromEquatorToEdge = RemapValClamped( flFireAboveSmokeZ, SmokeGrenadeRadius_InfernoAffectingZ * 0.6f, SmokeGrenadeRadius_InfernoAffectingZ, 0.0f, 1.0f );
		flPctFromEquatorToEdge *= flPctFromEquatorToEdge; // 0.0 still equator; 1.0 edge (squaring makes things feel quadratically closer to equator)
		flRadiusSquaredTest = RemapValClamped( flPctFromEquatorToEdge, 0.0f, 1.0f, flRadiusSquaredTest, SmokeGrenadeRadius_InfernoAffectingXY_topedge*SmokeGrenadeRadius_InfernoAffectingXY_topedge );
	}
	else if ( flFireAboveSmokeZ < SmokeGrenadeRadius_InfernoAffectingZ * 0.15f )
	{
		float flPctFromEquatorToEdge = RemapValClamped( flFireAboveSmokeZ, SmokeGrenadeRadius_InfernoAffectingZ * 0.1f, -flFireUpToSmokeCheckHeight, 0.0f, 1.0f );
		flPctFromEquatorToEdge *= flPctFromEquatorToEdge; // 0.0 still equator; 1.0 edge (squaring makes things feel quadratically closer to equator)
		flRadiusSquaredTest = RemapValClamped( flPctFromEquatorToEdge, 0.0f, 1.0f, flRadiusSquaredTest, SmokeGrenadeRadius_InfernoAffectingXY_bottomedge*SmokeGrenadeRadius_InfernoAffectingXY_bottomedge );
	}

	// Check if it is within XY-plane radius now
	vec_t lenXYsqr = ( vecFirePoint - vecSmokeOrigin ).Length2DSqr();
	return lenXYsqr <= flRadiusSquaredTest;
}


//------------------------------------------------------------------------------------------
CInferno::CInferno() :
	m_pWeaponInfo( NULL )
{
	// Set max flames to default in case the user doesn't ask for
	// more or less max flames for this inferno.
	SetMaxFlames( InfernoMaxFlames.GetInt() );
	ListenForGameEvent( "hegrenade_detonate" );
	ListenForGameEvent( "smokegrenade_detonate" );
	m_bWasCreatedInSmoke = false;
}


//------------------------------------------------------------------------------------------
CInferno::~CInferno()
{
	for ( int i = 0; i < m_fireCount; i++ )
	{
		delete m_fire[i];
		m_fire[i] = NULL;
	}

	
	switch( GetInfernoType() )
	{
	case INFERNO_TYPE_FIRE:
	case INFERNO_TYPE_INCGREN_FIRE:
		EmitSound( "Inferno.FadeOut" );
		StopSound( "Inferno.Loop" );
		break;
	case INFERNO_TYPE_FIREWORKS:
		EmitSound( "FireworksCrate.Stop" );
		StopSound( "FireworksCrate.Start" );
		break;
	}		
}


//------------------------------------------------------------------------------------------
void CInferno::Precache( void )
{
	// extend
	BaseClass::Precache();

	PrecacheScriptSound( "Inferno.Start" );
	PrecacheScriptSound( "Inferno.Start_IncGrenade" );
	PrecacheScriptSound( "Inferno.StartSweeten" );
	PrecacheScriptSound( "Inferno.Loop" );
	PrecacheScriptSound( "Inferno.Fire.Ignite" );
	PrecacheScriptSound( "Inferno.FadeOut" );

	PrecacheParticleSystem( "extinguish_fire" );
	PrecacheParticleSystem( "extinsguish_fire_blastout_01" );
	
	PrecacheScriptSound( "Molotov.Throw" );

	PrecacheScriptSound( "FireworksCrate.Start" );
	PrecacheScriptSound( "FireworksCrate.Stop" );

	if( GetParticleEffectName() != NULL )
	{
		PrecacheParticleSystem( GetParticleEffectName() );
	}

	if( GetImpactParticleEffectName() != NULL )
	{
		PrecacheParticleSystem( GetImpactParticleEffectName() );
	}
}

//------------------------------------------------------------------------------------------
void CInferno::Spawn( void )
{
	m_fireCount = 0;

	const float damageRampUpTime = 2.0f;
	m_damageRampTimer.Start( damageRampUpTime );

	SetThink( &CInferno::InfernoThink );
	SetNextThink( gpGlobals->curtime );

	m_NextSpreadTimer.Start( GetFlameSpreadDelay() );

	AddFlag( FL_ONFIRE );

	SetInfernoType( INFERNO_TYPE_FIRE );
}

//------------------------------------------------------------------------------------------
float CInferno::GetDamagePerSecond()
{
	return InfernoDamage.GetFloat();
}

//------------------------------------------------------------------------------------------
float CInferno::GetFlameLifetime() const
{
	return InfernoFlameLifetime.GetFloat();
}

void CInferno::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

// 	if ( Q_strcmp( "hegrenade_detonate", eventname ) == 0 )
// 	{
// 		Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), ( float )event->GetInt( "z" ) );
// 		float flGrenadeRadius = HEGrenadeRadius*1.5;
// 
// 		if ((vecGrenade - m_startPos).IsLengthLessThan( flGrenadeRadius*4 ))
// 		{
// 			ExtinguishFlamesInSphere( vecGrenade, flGrenadeRadius );
// 		}
// 	}

	if ( Q_strcmp( "smokegrenade_detonate", eventname ) == 0 )
	{
		Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), ( float )event->GetInt( "z" ) );
		if ((vecGrenade - m_startPos).IsLengthLessThan( SmokeGrenadeRadius*4 ))
		{
			int extinguishCount = ExtinguishFlamesAroundSmokeGrenade( vecGrenade );

			if ( extinguishCount == m_fireCount )
			{
				CSmokeGrenadeProjectile* pEnt = (CSmokeGrenadeProjectile*)CBaseEntity::Instance( INDEXENT( event->GetInt( "entityid" ) ) );
				if ( pEnt )
				{
					// If we were extinguished by a detonating smoke grenade, record it so we can report to OGS
					pEnt->m_unOGSExtraFlags |= CBaseCSGrenadeProjectile::GRENADE_EXTINGUISHED_INFERNO; 
				}
			}
		}
	}
}

//------------------------------------------------------------------------------------------
/**
 * Start the Inferno burning
 */
void CInferno::StartBurning( const Vector &pos, const Vector &normal, const Vector &velocity, int initialDepth )
{
	m_startPos.x = pos.x + InfernoSurfaceOffset.GetFloat() * normal.x;
	m_startPos.y = pos.y + InfernoSurfaceOffset.GetFloat() * normal.y;
	m_startPos.z = pos.z;

	// reflect velocity off of surface
	float splash = DotProduct( velocity, normal );
	Vector remainder = velocity - normal * splash;

	m_splashVelocity = remainder - InfernoVelocityNormalFactor.GetFloat() * normal * splash;

	QAngle splashangle;
	VectorAngles( velocity, splashangle );

	if( GetImpactParticleEffectName() != NULL )
	{
		DispatchParticleEffect( GetImpactParticleEffectName(), pos, splashangle );
	}

	if( InfernoDebug.GetBool() )
	{
		NDebugOverlay::Sphere( pos, 0.5f * InfernoFire_HalfWidth, 0, 255, 0, true, 10.0f);
		NDebugOverlay::Sphere( m_startPos, 0.5f * InfernoFire_HalfWidth, 255, 255, 0, true, 10.0f);
	}
	
	// create the initial bonfire that begins to spread
	if ( k_ECreateFireResult_OK == CreateFire( m_startPos, normal, NULL, initialDepth ) )
	{
		switch( GetInfernoType() )
		{
		case INFERNO_TYPE_FIRE:
			EmitSound( "Inferno.Start" );
			EmitSound( "Inferno.StartSweeten" );
			EmitSound( "Inferno.Loop" );
			break;
		case INFERNO_TYPE_INCGREN_FIRE:
			EmitSound( "Inferno.Start_IncGrenade" );
			EmitSound( "Inferno.StartSweeten_IncGrenade" );
			EmitSound( "Inferno.Loop" );
			break;
		case INFERNO_TYPE_FIREWORKS:
			EmitSound( "FireworksCrate.Start" );
			break;
		}

		m_startPos = m_fire[0]->m_pos;
		SetAbsOrigin( m_startPos );

		IGameEvent * event = gameeventmanager->CreateEvent( "inferno_startburn" );
		if ( event )
		{
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", m_startPos.x );
			event->SetFloat( "y", m_startPos.y );
			event->SetFloat( "z", m_startPos.z );
			gameeventmanager->FireEvent( event );
		}

		m_activeTimer.Start();
	}
	else
	{
		EmitSound( "Molotov.Extinguish" );
		DispatchParticleEffect( "extinguish_fire", m_startPos, splashangle );
		UTIL_Remove( this );
	}
}


class CInfernoLOSTraceFilter : public CTraceFilter
{
	// Find which objects can block molotov spreads
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );

		// Players do not block spread
		if ( pEntity->IsPlayer() )
			return false;
		
		// Chickens, hostages, and other 'navigating' objects don't block spread
		if( dynamic_cast< CImprovLocomotor* >( pEntity ) != nullptr )
			return false;

		// Other objects (doors, terrain, etc.) block spread
		return true;
	}
};

//------------------------------------------------------------------------------------------
/**
 * Spread the flames
 */
void CInferno::Spread( const Vector &spreadVelocity )
{
	if( m_NextSpreadTimer.HasStarted() && !m_NextSpreadTimer.IsElapsed() )
	{
		return;
	}

	m_NextSpreadTimer.Start( GetFlameSpreadDelay() );

	for ( int i = 0; i < m_fireCount; i++ )
	{
		// attempt to spawn child-flames
		FireInfo *fire = m_fire[ i ];
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't be spreading from here

		if ( !fire->m_spawnLifetime.IsElapsed() && fire->m_spawnTimer.IsElapsed() )
		{
			fire->m_spawnTimer.Reset();
			fire->m_spawnCount++;
		}
	}

	int traceCount = inferno_max_trace_per_tick.GetInt();
	int nextFireOffset = m_fireSpawnOffset + 1;
	for ( int i = 0; i < m_fireCount && traceCount > 0; i++ )
	{
		if ( m_fireCount >= MIN( ( int ) MAX_INFERNO_FIRES, m_nMaxFlames ) )
			break;

		int fireIndex = (i + m_fireSpawnOffset) % m_fireCount;
		FireInfo *fire = m_fire[fireIndex];
		nextFireOffset = fireIndex;
		if ( !fire->m_spawnCount )
			continue;
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't be spreading from here

		int depth = fire->m_treeDepth + 1;
		if ( depth >= InfernoChildSpawnMaxDepth.GetInt() )
			continue;

		fire->m_spawnCount--;

		trace_t tr;
		const int maxRetry = 4;
		for( int t=0; t<maxRetry; ++t )
		{
			Vector out;

			if (fire->m_parent == NULL)
			{
				// initial fire spreads outward in a circle
				float angle = random->RandomFloat( -3.14159f, 3.14159f );
				out = Vector( cos(angle), sin(angle), 0.0f );				
			}
			else
			{
				// child flames tend to spread away from their parent
				Vector to = fire->m_pos - fire->m_parent->m_pos;
				to.NormalizeInPlace();

				QAngle angles;
				VectorAngles( to, angles );

				angles.y += random->RandomFloat( -InfernoSpawnAngle.GetFloat(), InfernoSpawnAngle.GetFloat() );

				AngleVectors( angles, &out );
			}

			// If we're going into a wall, don't keep trying to spread into a wall the entire lifetime - back off to
			// a circular spread at the end.
			float velocityDecay = pow( InfernoVelocityDecayFactor.GetFloat(), float(fire->m_treeDepth) );
			Vector timeAdjustedSpreadVelocity = spreadVelocity * fire->m_lifetime.GetRemainingRatio() * velocityDecay;
			out += InfernoVelocityFactor.GetFloat() * timeAdjustedSpreadVelocity;

			// put fire on plane of ground
			Vector side = CrossProduct( fire->m_normal, out );
			out = CrossProduct( side, fire->m_normal );

			float range = random->RandomFloat( 50.0f, 75.0f );

			Vector pos = fire->m_pos + range * out;

			// limit maximum range of spread
			Vector fireDir = pos - m_startPos;
			if ( fireDir.IsLengthGreaterThan( InfernoMaxRange.GetFloat() ) )
			{
				VectorNormalize(fireDir);
				fireDir *= InfernoMaxRange.GetFloat();
				pos = m_startPos + fireDir;
			}

			// dont let flames fall too far
			const float maxDrop = 200.0f;
			Vector endPos = pos;
			endPos.z = fire->m_pos.z - maxDrop;

			// put fire on the ground
			UTIL_TraceLine( pos + Vector( 0, 0, 50.0f ), endPos, INFERNO_MASK_TO_GROUND, NULL, COLLISION_GROUP_NONE, &tr );
			traceCount--;
			if (!tr.DidHit())
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( pos + Vector( 0, 0, 50.0f ), endPos, 255, 255, 0, true, 1.0f );
					NDebugOverlay::Cross3D( pos, 5, 255, 0, 0, true, 1.0f );
				}
				m_splashVelocity *= inferno_forward_reduction_factor.GetFloat();
				continue;
			}
			pos.z = tr.endpos.z;
			Vector normal = tr.plane.normal;

			// make sure we dont go through walls
			const Vector fireHeight( 0, 0, InfernoFire_HalfWidth );
			CInfernoLOSTraceFilter losTraceFilter;
			UTIL_TraceLine( fire->m_pos + fireHeight, pos + fireHeight, INFERNO_MASK_LOS_CHECK, &losTraceFilter, &tr );
			traceCount--;
			if (tr.fraction < 1.0f)
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 0, 0, true, 1.0f );
				}
				m_splashVelocity *= inferno_forward_reduction_factor.GetFloat();
				continue;
			}

			ECreateFireResult_t eCreateFireResult = CreateFire( pos, normal, fire, depth );
			if ( ( eCreateFireResult == k_ECreateFireResult_OK )
				|| ( eCreateFireResult == k_ECreateFireResult_LimitExceeded ) )
				break;
			else if ( eCreateFireResult != k_ECreateFireResult_AlreadyOnFire )
				m_splashVelocity *= inferno_forward_reduction_factor.GetFloat();

			if ( InfernoDebug.GetBool() )
			{
				if ( eCreateFireResult == k_ECreateFireResult_InSmoke )
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 255, 0, true, 10.0f );
				else if ( eCreateFireResult == k_ECreateFireResult_AlreadyOnFire )
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 100, 100, true, 2.0f );
				else
					NDebugOverlay::Line( fire->m_pos + fireHeight, pos + fireHeight, 255, 100, 0, true, 10.0f );
			}
		}
	}
	m_fireSpawnOffset = nextFireOffset + 1;
}


//------------------------------------------------------------------------------------------
/**
 * Checks whether destination fire point is within a detonated smoke grenade cloud
 * This is useful to deny molly detonation in smoke, and to deny fire flames spreading into smoke
 */
bool CInferno::IsFirePosInSmokeCloud( const Vector &pos ) const
{
	const int INFERNO_SEARCH_ENTS = 32;
	CBaseEntity *pEntities[ INFERNO_SEARCH_ENTS ];
	int iNumEntities = UTIL_EntitiesInSphere( pEntities, INFERNO_SEARCH_ENTS, pos, SmokeGrenadeRadius, FL_GRENADE );
	for ( int i = 0; i < iNumEntities; i++ )
	{
		CSmokeGrenadeProjectile *pGrenade = dynamic_cast< CSmokeGrenadeProjectile * >( pEntities[ i ] );
		if ( pGrenade && pGrenade->m_bDidSmokeEffect
			&& BCheckFirePointInSmokeCloud( pos, pGrenade->GetAbsOrigin() ) )
		{
			return true;
		}
	}
	return false;
}


//------------------------------------------------------------------------------------------
/**
 * Create an actual fire entity at the given position
 */
CInferno::ECreateFireResult_t CInferno::CreateFire( const Vector &pos, const Vector &normal, FireInfo *parent, int depth )
{
	if ( m_fireCount >= MIN( ( int ) MAX_INFERNO_FIRES, m_nMaxFlames ) )
	{
		return k_ECreateFireResult_LimitExceeded;
	}

	if ( IsTouching( pos, pos, NULL ) )
	{
		// we already created a fire here
		return k_ECreateFireResult_AlreadyOnFire;
	}

	// if we throw down a molly in the middle of a smoke grenade, DENY!
	if( IsFirePosInSmokeCloud( pos ) )
	{
		m_bWasCreatedInSmoke = true;
		return k_ECreateFireResult_InSmoke;
	}

	if (InfernoDebug.GetBool())
	{
		if (parent)
		{
			NDebugOverlay::Line( parent->m_pos, pos, 0, 255, 255, true, 10.0f );
		}
	}


	Vector firePos( pos );
	bool overWater = false;

	trace_t tr;
	int contents = enginetrace->GetPointContents( pos, MASK_WATER );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME ) )
	{
		Vector fireHeight( 0, 0, 30.0f );

		int mask = MASK_SOLID_BRUSHONLY | CONTENTS_SLIME | CONTENTS_WATER;
		UTIL_TraceLine( pos + fireHeight, pos, mask, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.allsolid )
		{
			return k_ECreateFireResult_AllSolid;
		}
		else
		{
			firePos = tr.endpos;
			overWater = true;
		}
	}

	FireInfo *fire = new FireInfo;

	fire->m_pos = firePos;
	fire->m_center = firePos + Vector( 0, 0, 0.5f * InfernoFire_FullHeight );
	fire->m_normal = normal;
	fire->m_parent = parent;
	fire->m_treeDepth = depth;
	fire->m_spawnCount = 0;
	fire->m_flWaterHeight = firePos.z - pos.z;
	fire->m_burning = true;

	// all control points on the client die down at the same time, so the server needs to match this
	if ( m_activeTimer.HasStarted() )
	{
		fire->m_lifetime.Start( GetFlameLifetime() - m_activeTimer.GetElapsedTime() );
	}
	else
	{
		fire->m_lifetime.Start( GetFlameLifetime() );
	}

	if (parent)
	{
		fire->m_spawnLifetime.Start( parent->m_spawnLifetime.GetCountdownDuration() );

		float duration = InfernoChildSpawnIntervalMultiplier.GetFloat() * parent->m_spawnTimer.GetCountdownDuration();
		if (duration > InfernoMaxChildSpawnInterval.GetFloat())
			duration = InfernoMaxChildSpawnInterval.GetFloat();

		fire->m_spawnTimer.Start( duration );
	}
	else
	{
		fire->m_spawnLifetime.Start( InfernoPerFlameSpawnDuration.GetFloat() );
		fire->m_spawnTimer.Start( InfernoInitialSpawnInterval.GetFloat() );
	}

	// keep a simple array of all active fires
	m_fire[ m_fireCount ] = fire;

	// propogate across the network

	// Compute this fire's position relative to the Inferno entity.
	Vector vecDelta = fire->m_pos - GetAbsOrigin(); 

	m_fireXDelta.Set( m_fireCount, (int)vecDelta.x );
	m_fireYDelta.Set( m_fireCount, (int)vecDelta.y );
	m_fireZDelta.Set( m_fireCount, (int)vecDelta.z );
	m_bFireIsBurning.Set( m_fireCount, true );
	m_BurnNormal.Set( m_fireCount, normal );

	++m_fireCount;

	RecomputeExtent();
	
	// emit a small flame burst sound
	if( GetInfernoType() == INFERNO_TYPE_FIRE || GetInfernoType() == INFERNO_TYPE_INCGREN_FIRE )
	{
		CSoundParameters params;
		if ( GetParametersForSound( "Inferno.Fire.Ignite", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_pOrigin = &fire->m_pos;

			CBroadcastRecipientFilter filter;
			EmitSound( filter, SOUND_FROM_WORLD, ep );
		}

		if ( inferno_scorch_decals.GetBool() && !overWater )
		{
			trace_t trace;
			const float dist = 100.0f;
			Vector dir( 0, 0, -1 );
			UTIL_TraceLine( fire->m_pos, fire->m_pos + dir * dist, MASK_OPAQUE, NULL, COLLISION_GROUP_NONE, &trace );
			UTIL_DecalTrace( &trace, "MolotovScorch" );
		}
	}

	return k_ECreateFireResult_OK;
}

void CInferno::ExtinguishIndividualFlameBySmokeGrenade( int iFire, Vector vecStart )
{
	m_fire[ iFire ]->m_lifetime.Invalidate();

	Vector vecAngleAway = m_fire[ iFire ]->m_pos - vecStart;
	vecAngleAway.NormalizeInPlace();

	QAngle angParticle;
	VectorAngles( vecAngleAway, angParticle );

	DispatchParticleEffect( "extinguish_fire", m_fire[ iFire ]->m_pos, angParticle );
}

int CInferno::ExtinguishFlamesAroundSmokeGrenade( Vector vecStart )
{
	bool bExtinguished = false; 
	bool bCheckDistanceForFlames = true;
	int nNumExtinguished = 0;
	
	// if the radius overlaps the center, extinguish the whole flame
	if ( BCheckFirePointInSmokeCloud( m_startPos, vecStart ) )
		bCheckDistanceForFlames = false;

	for( int i=0; i<m_fireCount; ++i )
	{
		// if this fire just died, propagate over the network
		if ( m_fire[i]->m_burning && (
			!bCheckDistanceForFlames || BCheckFirePointInSmokeCloud( m_fire[i]->m_pos, vecStart )
			) )
		{
			ExtinguishIndividualFlameBySmokeGrenade( i, vecStart );
			bExtinguished = true;
			nNumExtinguished++;
		}
	}

	// if we extinguished third or more of our fire, just put out the rest
	if ( !bCheckDistanceForFlames && nNumExtinguished >= (m_fireCount/3) )
	{
		for( int i=0; i<m_fireCount; ++i )
		{
			// if this fire just died, propagate over the network
			if ( !m_fire[i]->m_lifetime.IsElapsed() )
			{
				ExtinguishIndividualFlameBySmokeGrenade( i, vecStart );
				nNumExtinguished++;
			}
		}
	}

	if ( bExtinguished )
	{
		EmitSound( "Molotov.Extinguish" );

		IGameEvent * event = gameeventmanager->CreateEvent( "inferno_extinguish" );
		if ( event )
		{
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", m_startPos.x );
			event->SetFloat( "y", m_startPos.y );
			event->SetFloat( "z", m_startPos.z );
			gameeventmanager->FireEvent( event );
		}
	}

	return nNumExtinguished; 
}

bool CInferno::CheckExpired()
{
	VPROF_BUDGET( "CInferno::CheckExpired (check lifetimes)", "Fire" );

	bool bIsAttachedToMovingObject = GetParent() != NULL;
	Vector vecInfernoOrigin = GetAbsOrigin();

	// check lifetime of flames
	bool isDone = true;
	for ( int i = 0; i < m_fireCount; ++i )
	{
		// Already dead.
		if ( !m_fire[i]->m_burning )
			continue;

		// if this fire just died, propagate over the network
		if ( m_fire[i]->m_lifetime.IsElapsed() )
		{
			m_fire[i]->m_pos = Vector( 0, 0, 0 );
			m_fire[i]->m_burning = false;
			m_bFireIsBurning.Set( i, false );
			continue;
		}

		// still at least one fire alive
		isDone = false;

		m_fire[i]->m_pos = vecInfernoOrigin;
		m_fire[i]->m_pos.x += m_fireXDelta[i];
		m_fire[i]->m_pos.y += m_fireYDelta[i];
		m_fire[i]->m_pos.z += m_fireZDelta[i];

		if ( bIsAttachedToMovingObject )
		{
			RecomputeExtent();
		}

		if ( InfernoDebug.GetBool() )
		{
			NDebugOverlay::Sphere( m_fire[i]->m_pos, 2.0f * InfernoFire_HalfWidth, 255, 100, 0, true, 0.1f );
		}
	}

	if ( isDone )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "inferno_expire" );
		if ( event )
		{
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", m_startPos.x );
			event->SetFloat( "y", m_startPos.y );
			event->SetFloat( "z", m_startPos.z );
			gameeventmanager->FireEvent( event );
		}

		// if all fires have burned out, we're done
		UTIL_Remove( this );

		// Expired!
		return true;
	}

	// Not expired
	return false;
}

void CInferno::MarkCoveredAreaAsDamaging()
{
	// mark overlapping nav areas as "damaging"
	NavAreaCollector overlap;

	// bloat extents enough to ensure any non-damaging area is actually safe
	// bloat in Z as well to catch nav areas that may be slightly above/below ground
	Extent extent = m_extent;
	float DangerBloat = 32.0f;

	Vector dangerBloat( DangerBloat, DangerBloat, DangerBloat );
	extent.lo -= dangerBloat;
	extent.hi += dangerBloat;

	//NDebugOverlay::Box( vec3_origin, extent.lo, extent.hi, 0, 255, 0, 10, 0.1f );

	TheNavMesh->ForAllAreasOverlappingExtent( overlap, extent );

	FOR_EACH_VEC( overlap.m_area, it )
	{
		CNavArea *area = overlap.m_area[it];

		if ( IsTouching( area ) )
		{
			area->MarkAsDamaging( 1.0f );
		}
	}
}

//------------------------------------------------------------------------------------------
/**
 * Spread the flames
 */
void CInferno::InfernoThink( void )
{
	VPROF_BUDGET( "CInferno::InfernoThink", "Fire" );

	bool bExpiryCheckPerformed = false;

	// Run bookkeeping every 0.1s
	if ( m_BookkeepingTimer.Interval(0.1f) )
	{
		bExpiryCheckPerformed = true;
		if ( CheckExpired() )
			return;

		// the fire grows...
		if ( m_fireCount > 0 && m_fireCount < MIN( m_nMaxFlames, ( int )MAX_INFERNO_FIRES ) )
		{
			VPROF_BUDGET( "CInferno::InfernoThink (spread)", "Fire" );
			Spread( m_splashVelocity );
		}

		// Mark area as damaging for bot avoidance
		MarkCoveredAreaAsDamaging();
	}

#if 0
	// Debug draw flame region
	NDebugOverlay::Box( vec3_origin, m_extent.lo, m_extent.hi, 255, 255, 255, 10, 0.1f );
#endif

	// Deal damage every 0.2s
	const float kDamageTimerSeconds = 0.2f;
	while ( m_damageTimer.RunEvery( kDamageTimerSeconds ) )
	{
		// Note that we run a lot of code in this RunEvery(), but we expect the loop to run 0 or 1 times
		// in almost every case, unless this think function somehow got super delayed by the server

		if(!bExpiryCheckPerformed)
		{
			bExpiryCheckPerformed = true;
			if ( CheckExpired() )
				return;
		}

		VPROF_BUDGET( "CInferno::InfernoThink (damage)", "Fire" );

		const int maxVictims = 256;
		CBaseEntity *damageList[ maxVictims ];
		CBaseEntity *owner = GetOwnerEntity();
		int damageCount = 0;
		const float flameRadius = 2.0f * InfernoFire_HalfWidth;

		CBaseEntity *list[ maxVictims ];
		int count = UTIL_EntitiesInBox( list, maxVictims, m_extent.lo, m_extent.hi, 0 );

		for( int i=0; i<count; ++i )
		{
			if (list[i] == NULL || !list[i]->IsAlive() || list[i] == this)
				continue;

			if (IsTouching( list[i], flameRadius, list[i]->IsPlayer() ))
			{
				damageList[damageCount] = list[i];
				damageCount++;
			}
		}

		int damageType = GetDamageType();
#if !defined( CSTRIKE15 )
		// After the first few seconds of burning, the thrower isn't responsible for teammates who run into the fire.
		if ( m_activeTimer.GetElapsedTime() > InfernoFriendlyFireDuration.GetFloat() || owner == NULL )
		{
			damageType |= DMG_BLAMELESS_FRIENDLY_FIRE; // Add in a flag to prevent FF demerits
		}
#endif

		// Note that we expect molotov this value to be an integer (currently it is 40 * 0.2 == 8 damage per tick)
		// If molotov DPS changes, we may need to also adjust how often damage is applied.
		//
		// We could also change this to tick at the exact rate required to deal 1 damage (tick rate = 1 / GetDamagePerSecond())
		// at which point we might want to consider optimizing this loop to run a single time and multiply its damage
		// by the damage dealt, so that (for example) if molotovs deal 80 dps on a 64 tick server, we only find the targets
		// once during the ticks when the molotov deals 2 damage.
		float baseDamage = GetDamagePerSecond() * kDamageTimerSeconds; // dmg / sec * sec / tick = dmg / tick

		if ( !m_damageRampTimer.IsElapsed() )
		{
			baseDamage *= m_damageRampTimer.GetElapsedRatio();
		}

		for ( int i = 0; i < damageCount; i++ )
		{
			// damage the victim
			CBaseEntity *pEnt = damageList[i];
			
			float damage = baseDamage;

			if( CanHarm( pEnt ) )
			{
				CTakeDamageInfo info( this, owner, damage, damageType );

				pEnt->TakeDamage( info );

			}
		}
	}

	// Figure out when our next event is and make sure we get a think tick close to it.
	float nextThink = m_BookkeepingTimer.GetTargetTime();
	nextThink = MIN( nextThink, m_damageTimer.GetTargetTime() );
	SetNextThink( nextThink );
}


//-----------------------------------------------------------------------------------------------
void CInferno::RecomputeExtent( void )
{
	m_extent.lo = Vector( 999999.9f, 999999.9f, 999999.9f );
	m_extent.hi = Vector( -999999.9f, -999999.9f, -999999.9f );

	for( int i=0; i<m_fireCount; ++i )
	{
		FireInfo *fire = m_fire[i];

		if ( fire->m_pos.x - InfernoFire_HalfWidth < m_extent.lo.x )
			m_extent.lo.x = fire->m_pos.x - InfernoFire_HalfWidth;

		if ( fire->m_pos.x + InfernoFire_HalfWidth > m_extent.hi.x )
			m_extent.hi.x = fire->m_pos.x + InfernoFire_HalfWidth;

		if ( fire->m_pos.y - InfernoFire_HalfWidth < m_extent.lo.y )
			m_extent.lo.y = fire->m_pos.y - InfernoFire_HalfWidth;

		if ( fire->m_pos.y + InfernoFire_HalfWidth > m_extent.hi.y )
			m_extent.hi.y = fire->m_pos.y + InfernoFire_HalfWidth;

		if ( fire->m_pos.z < m_extent.lo.z )
			m_extent.lo.z = fire->m_pos.z;

		if ( fire->m_pos.z + InfernoFire_FullHeight > m_extent.hi.z )
			m_extent.hi.z = fire->m_pos.z + InfernoFire_FullHeight;
	}
}


bool CInferno::BShouldExtinguishSmokeGrenadeBounce( CBaseEntity *entity, Vector &posDropSmoke ) const
{
	const float radius = 2.0f * InfernoFire_HalfWidth;
	for ( int i = 0; i < m_fireCount; i++ )
	{
		FireInfo *fire = m_fire[ i ];
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't cause damage

		if ( ( posDropSmoke - fire->m_center ).IsLengthLessThan( radius ) )
		{
			// doublecheck los if required
			trace_t tr;
			const Vector fireHeight( 0, 0, InfernoFire_HalfWidth );
			UTIL_TraceLine( fire->m_center + fireHeight, posDropSmoke, INFERNO_MASK_DAMAGE, entity, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction < 1.0f )
				UTIL_TraceLine( fire->m_center, posDropSmoke, INFERNO_MASK_DAMAGE, entity, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction == 1.0f )
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( fire->m_center, posDropSmoke, 255, 0, 255, true, 50.2f );
				}

				return true;
			}
			else
			{
				if ( InfernoDebug.GetBool() )
				{
					NDebugOverlay::Line( fire->m_center, posDropSmoke, 255, 0, 0, true, 50.2f );
				}
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if position is in contact with a fire within the Inferno
 */
bool CInferno::IsTouching( CBaseEntity *entity, float radius, bool checkLOS ) const
{
	if ( entity != NULL )
	{
		for ( int i = 0; i < m_fireCount; i++ )
		{
			FireInfo *fire = m_fire[i];
			if ( !fire->m_burning ||
				fire->m_lifetime.IsElapsed() )
				continue;	// This flame has been extinguished or elapsed, shouldn't cause damage

			// Calculate the nearest point to our potential victim, from our center point
			const Vector fireHeight( 0, 0, InfernoFire_HalfWidth );

			Vector pos;
			Vector fireCheck = fire->m_center;
			if( checkLOS )
				fireCheck += fireHeight;
			entity->CollisionProp()->CalcNearestPoint( fireCheck, &pos );

			if ( ( pos - fireCheck ).IsLengthLessThan( radius ) )
			{
				// touching at least one flame
				if( checkLOS )
				{
					// doublecheck los if required
					trace_t tr;
					UTIL_TraceLine( fireCheck, pos, INFERNO_MASK_DAMAGE, entity, COLLISION_GROUP_NONE, &tr );
					if ( tr.fraction < 1.0f )
					{
						fireCheck = fire->m_center;
						entity->CollisionProp()->CalcNearestPoint( fireCheck, &pos );
						if ( ( pos - fireCheck ).IsLengthLessThan( radius ) )
							UTIL_TraceLine( fireCheck, pos, INFERNO_MASK_DAMAGE, entity, COLLISION_GROUP_NONE, &tr );
					}
					if( tr.fraction == 1.0f )
					{
						if( InfernoDebug.GetBool() )
						{
							NDebugOverlay::Line( fire->m_center, pos, 255, 0, 255, true, 50.2f );
						}

						return true;
					}
					else
					{
						if( InfernoDebug.GetBool() )
						{
							NDebugOverlay::Line( fire->m_center, pos, 255, 0, 0, true, 50.2f );
						}
					}
				}
				else
				{
					// los not needed, it's touching
					if( InfernoDebug.GetBool() )
					{
						NDebugOverlay::Line( fire->m_center, pos, 255, 0, 255, true, 0.2f );
					}

					return true;
				}
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if given ray intersects any fires
 * TODO: Check LOS if needed.
 */
bool CInferno::IsTouching( const Vector &from, const Vector &to, Vector *where ) const
{
	for ( int i = 0; i < m_fireCount; i++ )
	{
		FireInfo *fire = m_fire[i];
		if ( !fire->m_burning ||
			fire->m_lifetime.IsElapsed() )
			continue;	// This flame has been extinguished or elapsed, shouldn't be considered touching

		Vector pointOnRay;
		ClosestPointOnRay( m_fire[i]->m_center, from, to, &pointOnRay );

		const float radius = 2.0f * InfernoFire_HalfWidth;
		if ( ( pointOnRay - m_fire[i]->m_center ).IsLengthLessThan( radius ) )
		{
			if ( where )
			{
				*where = pointOnRay;
			}
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------------------------
/**
 * Return true if given area overlaps any fires
 */
bool CInferno::IsTouching( const CNavArea *area ) const	
{
	if ( area != NULL )
	{
		float radius = 2.0f * InfernoFire_HalfWidth;

		for ( int i = 0; i < m_fireCount; i++ )
		{
			FireInfo *fire = m_fire[ i ];
			if ( !fire->m_burning ||
				fire->m_lifetime.IsElapsed() )
				continue;	// This flame has been extinguished or elapsed, shouldn't be considered touching

			Vector close;
			area->GetClosestPointOnArea( m_fire[i]->m_center, &close );

			close.z += m_fire[i]->m_flWaterHeight;	// If the inferno was raised above the water, check the nav as if it was in the original pos

			if ( ( close - m_fire[i]->m_center ).IsLengthLessThan( radius ) )
			{
				return true;
			}
		}
	}
	return false;
}


//------------------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------------------
void CFireCrackerBlast::Spawn( void )
{
	BaseClass::Spawn();
	SetInfernoType( INFERNO_TYPE_FIREWORKS );
}
