//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "sensorgrenade_projectile.h"
#include "engine/IEngineSound.h"
#include "keyvalues.h"
#include "weapon_csbase.h"
#include "particle_parse.h"

#if defined( CLIENT_DLL )

	#include "c_cs_player.h"
#else
	#include "sendproxy.h"
	#include "cs_player.h"
	#include "bot_manager.h"
	#include "cs_bot.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#if defined( CLIENT_DLL )

IMPLEMENT_CLIENTCLASS_DT( C_SensorGrenadeProjectile, DT_SensorGrenadeProjectile, CSensorGrenadeProjectile )
END_RECV_TABLE()


//--------------------------------------------------------------------------------------------------------
void C_SensorGrenadeProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, "weapon_sensorgren_detlight" ) )
	{
		m_sensorgrenadeParticleEffect = pNewParticleEffect;
	}
}


//--------------------------------------------------------------------------------------------------------
void C_SensorGrenadeProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_sensorgrenadeParticleEffect == pParticleEffect )
	{
		m_sensorgrenadeParticleEffect = NULL;
	}
}


//--------------------------------------------------------------------------------------------------------
bool C_SensorGrenadeProjectile::Simulate( void )
{
	// we are still moving
	if ( GetAbsVelocity().Length() > 0.1f )
	{
		return true;
	}

	if ( !m_sensorgrenadeParticleEffect.IsValid() )
	{
		DispatchParticleEffect( "weapon_sensorgren_detlight", PATTACH_POINT_FOLLOW, this, "Wick" );
	}
	else
	{
		m_sensorgrenadeParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_sensorgrenadeParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}

#else // GAME_DLL

#define GRENADE_MODEL "models/Weapons/w_eq_sensorgrenade_thrown.mdl"

LINK_ENTITY_TO_CLASS( tagrenade_projectile, CSensorGrenadeProjectile );
PRECACHE_REGISTER( tagrenade_projectile );

IMPLEMENT_SERVERCLASS_ST( CSensorGrenadeProjectile, DT_SensorGrenadeProjectile )
END_SEND_TABLE()

BEGIN_DATADESC( CSensorGrenadeProjectile )
	DEFINE_THINKFUNC( Think_Arm ),
	DEFINE_THINKFUNC( Think_Remove ),
	DEFINE_THINKFUNC( SensorThink )
END_DATADESC()


// --------------------------------------------------------------------------------------------------- //
// CFlashbangProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CSensorGrenadeProjectile* CSensorGrenadeProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CSensorGrenadeProjectile *pGrenade = ( CSensorGrenadeProjectile* )CBaseEntity::Create( "tagrenade_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetTimer( 2.0f );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );

	pGrenade->m_flDamage = 1.0f; // 25 = 1/4 of HEGrenade Damage
	pGrenade->m_DmgRadius = pGrenade->m_flDamage * 3.5f; // Changing damage will change the radius
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->SetAbsAngles( pOwner->EyeAngles() + QAngle( -80, 40, 0 ) );
	QAngle angRotationVel = QAngle( RandomFloat(100,200), RandomFloat(-100,200), RandomFloat(-100,200) );
	pGrenade->SetLocalAngularVelocity( angRotationVel );
	pGrenade->SetTouch( &CSensorGrenadeProjectile::BounceTouch );

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );

	pGrenade->m_pWeaponInfo = &weaponInfo;

	ASSERT(pOwner != NULL);

	//pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CSensorGrenadeProjectile::SetTimer( float timer )
{
	SetThink( &CSensorGrenadeProjectile::Think_Arm );
	SetNextThink( gpGlobals->curtime + timer );

	m_fNextDetectPlayerSound = gpGlobals->curtime;

	TheBots->SetGrenadeRadius( this, 0.0f );
}

void CSensorGrenadeProjectile::Think_Arm( void )
{
#if 1
	if ( GetAbsVelocity().Length() > 0.2f )
	{
		// Still moving. Don't detonate yet.
		SetNextThink( gpGlobals->curtime + 0.2f );
		return;
	}
#endif // 0

	m_fExpireTime = gpGlobals->curtime + 2.0f; // TODO: Make this Data Driven

	SetThink( &CSensorGrenadeProjectile::SensorThink );
	//TheBots->SetGrenadeRadius( this, SensorGrenadeGrenadeRadius );
	SensorThink(); // This will handling the 'Detonate'
}

void CSensorGrenadeProjectile::Think_Remove( void )
{
	UTIL_Remove( this );
}

void CSensorGrenadeProjectile::Detonate( void )
{
	// [mlowrance] The SensorGrenade is handling it's own detonate.
	Assert(!"SensorGrenade grenade handles its own detonation");
}

void CSensorGrenadeProjectile::SensorThink( void )
{
	// tell the bots about the gunfire
 	CCSPlayer *pThrower = ToCSPlayer( GetThrower() );
	if ( !pThrower )
		return;

	if ( gpGlobals->curtime > m_fNextDetectPlayerSound )
	{
		EmitSound( "Sensor.WarmupBeep" );
		m_fNextDetectPlayerSound = gpGlobals->curtime + 1.0f; // TODO: Make this Data Driven
	}

	if ( gpGlobals->curtime < m_fExpireTime )
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		// [mlowrance] Do the damage on Despawn and post event
		CCSPlayer *player = ToCSPlayer( GetThrower() );
		if ( player )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "tagrenade_detonate" );
			if ( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				event->SetInt( "entityid", this->entindex() );
				event->SetFloat( "x", GetAbsOrigin().x );
				event->SetFloat( "y", GetAbsOrigin().y );
				event->SetFloat( "z", GetAbsOrigin().z );
				gameeventmanager->FireEvent( event );
			}
		}

		TheBots->RemoveGrenade( this );

		DispatchParticleEffect( "weapon_sensorgren_detonate", PATTACH_POINT, this, "Wick" );
		EmitSound( "Sensor.Detonate" );	

		DoDetectWave();

		//BaseClass::Detonate();
	}
}

void CSensorGrenadeProjectile::DoDetectWave( void )
{
	// tell the bots about the gunfire
	CCSPlayer *pThrower = ToCSPlayer( GetThrower() );
	if ( !pThrower )
		return;

	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( !pPlayer || !pPlayer->IsAlive() || !pThrower->IsOtherEnemy( pPlayer ) )
			continue;

		Vector vDelta = pPlayer->EyePosition() - GetAbsOrigin();
		float flDistance = vDelta.Length();

		float flMaxTraceDist = 1600;
		if ( flDistance <= flMaxTraceDist )
		{
			trace_t tr;
			//if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
			{
				if ( TheCSBots()->IsLineBlockedBySmoke( pPlayer->EyePosition(), GetAbsOrigin(), 1.0f ) )
				{
					// if we are outside half the max dist and don't trace, dont show
					if ( flDistance > (flMaxTraceDist/2) )
						continue;
				}

				UTIL_TraceLine( pPlayer->EyePosition(), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr );
				if ( tr.fraction != 1 )
				{
					trace_t tr2;
					UTIL_TraceLine( pPlayer->GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr2 );
					if ( tr2.fraction != 1 )
					{
						// if we are outside half the max dist and don't trace, dont show
						if ( flDistance > (flMaxTraceDist/2) )
							continue;
					}
				}

				int nThrowerIndex = 0;
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
					if ( pPlayer == pThrower )
					{
						nThrowerIndex = i;
						break;
					}
				}

				DebugDrawLine( WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), 90, 0, 0, true, 1.5f );
				pPlayer->SetIsSpotted( true );
				pPlayer->SetIsSpottedBy( nThrowerIndex );
				pPlayer->m_flDetectedByEnemySensorTime = gpGlobals->curtime;
				pPlayer->Blind( 0.02f, 1.0f, 128 );
				pPlayer->EmitSound( "Sensor.WarmupBeep" );
			}
		}
	}

	SetNextThink( gpGlobals->curtime + 0.25f );
	SetThink( &CSensorGrenadeProjectile::Think_Remove );
}

void CSensorGrenadeProjectile::Spawn( void )
{
	SetModel( GRENADE_MODEL );
 	BaseClass::Spawn();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
}

void CSensorGrenadeProjectile::Precache( void )
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Sensor.Detonate" );
	PrecacheScriptSound( "Sensor.WarmupBeep" );
	PrecacheScriptSound( "Sensor.Activate" );
	PrecacheScriptSound( "Flashbang.Bounce" );

	//PrecacheParticleSystem( "weapon_sen_active" );
	PrecacheParticleSystem( "weapon_sensorgren_detlight" );
	PrecacheParticleSystem( "weapon_sensorgren_detonate" );

	BaseClass::Precache();
}

void CSensorGrenadeProjectile::BounceTouch( CBaseEntity *other )
{
	if ( other->IsSolidFlagSet( FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS ) )
		return;

	// don't hit the guy that launched this grenade
	if ( other == GetThrower() )
		return;

	if ( FClassnameIs( other, "func_breakable" ) )
	{
		return;
	}

	if ( FClassnameIs( other, "func_breakable_surf" ) )
	{
		return;
	}

	// don't detonate on ladders
	if ( FClassnameIs( other, "func_ladder" ) )
	{
		return;
	}

	// Deal car alarms direct damage to set them off - flames won't do so
	if ( FClassnameIs( other, "prop_car_alarm" ) || FClassnameIs( other, "prop_car_glass" ) )
	{
		CTakeDamageInfo info( this, GetThrower(), 10, DMG_GENERIC );
		other->OnTakeDamage( info );
	}

	const trace_t &hitTrace = GetTouchTrace();
	if ( hitTrace.m_pEnt && hitTrace.m_pEnt->MyCombatCharacterPointer() )
	{
		// don't break if we hit an actor - wait until we hit the environment
		return;
	}
	else
	{
		SetAbsVelocity( Vector( 0, 0, 0) );
		SetMoveType(MOVETYPE_NONE);
		SetNextThink( gpGlobals->curtime + 1.0f );
		SetThink( &CSensorGrenadeProjectile::Think_Arm );

		EmitSound( "Sensor.Activate" );

		m_fExpireTime = gpGlobals->curtime + 15.0f; // TODO: Make this Data Driven



		// stick the grenade onto the target surface using the closest rotational alignment to match the in-flight orientation,
		// ( like breach charges )

		Vector vecSurfNormal = hitTrace.plane.normal.Normalized();
		Vector vecProjectileZ = EntityToWorldTransform().GetColumn(Z_AXIS);

		// sensor grenades can stick on either of two sides, unlike the breach charges. So they don't need to flip when they land on their 'backs'.
		if ( DotProduct( vecSurfNormal, vecProjectileZ ) < 0 )
			vecSurfNormal = -vecSurfNormal;

		QAngle angSurface;
		MatrixAngles( ConcatTransforms( QuaternionMatrix( RotateBetween( vecProjectileZ, vecSurfNormal ) ), EntityToWorldTransform() ), angSurface );
		SetAbsAngles( angSurface );


		//if ( fabs(hitTrace.plane.normal.Dot(Vector(0,0,1))) > 0.65f )
		{
			//get the player forward vector
// 			Vector vecFlatForward;
// 			VectorCopy( pPlayer->Forward(), vecFlatForward );
// 			vecFlatForward.z = 0;
// 
// 			//derive c4 forward and right
// 			Vector vecC4Right = CrossProduct( vecFlatForward.Normalized(), hitTrace.plane.normal );
// 			Vector vecC4Forward = CrossProduct( vecC4Right, trPlant.plane.normal );

			//QAngle angle;
			//VectorAngles( hitTrace.plane.normal, angle );
			//SetAbsAngles( angle );

			//m_hDisplayGrenade = CreatePhysicsProp( GRENADE_MODEL, GetAbsOrigin(), GetAbsOrigin(), NULL, false, "prop_physics_multiplayer" );

			/*
			m_hDisplayGrenade = ( CBreakableProp * )CBaseEntity::CreateNoSpawn( "prop_physics", GetAbsOrigin(), angle );
			CBreakableProp *pDisplay = dynamic_cast< CBreakableProp* >( m_hDisplayGrenade.Get() );
			if ( pDisplay )
			{
				pDisplay->KeyValue( "fademindist", "-1" );
				pDisplay->KeyValue( "fademaxdist", "0" );
				pDisplay->KeyValue( "fadescale", "1" );
				pDisplay->KeyValue( "inertiaScale", "1.0" );
				pDisplay->KeyValue( "physdamagescale", "0.1" );
				//pDisplay->SetPhysicsMode( PHYSICS_MULTIPLAYER_SOLID );
				pDisplay->SetModel( GRENADE_MODEL );
				pDisplay->SetSolid( SOLID_BBOX );
				pDisplay->AddSolidFlags( FSOLID_NOT_STANDABLE );
				pDisplay->AddSpawnFlags( SF_PHYSPROP_MOTIONDISABLED );
				pDisplay->Precache();
				DispatchSpawn( pDisplay );
				pDisplay->Activate();

				// disable the parent model
				SetModelName( NULL_STRING );//invisible
				SetSolid( SOLID_NONE );
			}
			*/
		}

		/*
		// only detonate on surfaces less steep than this
		const float kMinCos = cosf( DEG2RAD( weapon_molotov_maxdetonateslope.GetFloat() ) );
		if ( hitTrace.plane.normal.z >= kMinCos )
		{
			//Stick();

		}
		*/
	}
}

//TODO: Let physics handle the sound!
void CSensorGrenadeProjectile::BounceSound( void )
{
	EmitSound( "Flashbang.Bounce" );
}

#endif // GAME_DLL
