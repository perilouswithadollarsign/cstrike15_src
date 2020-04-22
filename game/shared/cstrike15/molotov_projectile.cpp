//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "molotov_projectile.h"

#include "keyvalues.h"
#include "weapon_csbase.h"
#include "particle_parse.h"

#if defined( CLIENT_DLL )
	#include "particle_parse.h"
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
	#include "smoke_trail.h"
	#include "Effects/inferno.h"
	#include "bot_manager.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar molotov_throw_detonate_time( "molotov_throw_detonate_time", "2.0", FCVAR_CHEAT | FCVAR_REPLICATED );

#if defined( CLIENT_DLL )

IMPLEMENT_CLIENTCLASS_DT( C_MolotovProjectile, DT_MolotovProjectile, CMolotovProjectile )
	RecvPropBool( RECVINFO(m_bIsIncGrenade) ),
END_RECV_TABLE()



//--------------------------------------------------------------------------------------------------------
void C_MolotovProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, "weapon_molotov_thrown" ) || FStrEq( pszParticleName, "incgrenade_thrown_trail" ) )
	{
		m_molotovParticleEffect = pNewParticleEffect;
	}
}


//--------------------------------------------------------------------------------------------------------
void C_MolotovProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_molotovParticleEffect == pParticleEffect )
	{
		m_molotovParticleEffect = NULL;
	}
}


//--------------------------------------------------------------------------------------------------------
bool C_MolotovProjectile::Simulate( void )
{
	if ( !m_molotovParticleEffect.IsValid() )
	{
		if ( m_bIsIncGrenade )
			// todo: make this come from an attachment
			DispatchParticleEffect( "incgrenade_thrown_trail", PATTACH_POINT_FOLLOW, this, "trail" );
		else
			DispatchParticleEffect( "weapon_molotov_thrown", PATTACH_POINT_FOLLOW, this, "Wick" );
	}
	else
	{
		m_molotovParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_molotovParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}


#else // GAME_DLL

ConVar weapon_molotov_maxdetonateslope( 
	"weapon_molotov_maxdetonateslope",
	"30.0",
	FCVAR_REPLICATED,
	"Maximum angle of slope on which the molotov will detonate",
	true, 0.0,
	true, 90.0 );


#define MOLOTOV_MODEL "models/Weapons/w_eq_molotov_dropped.mdl"
#define INCGREN_MODEL "models/Weapons/w_eq_incendiarygrenade_dropped.mdl"


LINK_ENTITY_TO_CLASS( molotov_projectile, CMolotovProjectile );
PRECACHE_REGISTER( molotov_projectile );

BEGIN_DATADESC( CMolotovProjectile )

// Inputs
DEFINE_INPUTFUNC( FIELD_VOID, "InitializeSpawnFromWorld", InitializeSpawnFromWorld ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CMolotovProjectile, DT_MolotovProjectile )
	SendPropBool( SENDINFO(m_bIsIncGrenade) ),
END_SEND_TABLE()

CMolotovProjectile *CMolotovProjectile::Create( const Vector &position, const QAngle &angles, 
												const Vector &velocity, const AngularImpulse &angVelocity, 
												CBaseCombatCharacter *owner, const CCSWeaponInfo& weaponInfo )
{
	CMolotovProjectile *molotov = (CMolotovProjectile *)CBaseEntity::Create( "molotov_projectile", position, angles, owner );
	UTIL_LogPrintf( "Molotov projectile spawned at %f %f %f, velocity %f %f %f\n", position.x, position.y, position.z, velocity.x, velocity.y, velocity.z );

	molotov->SetDetonateTimerLength( molotov_throw_detonate_time.GetFloat() );
	molotov->SetAbsVelocity( velocity );
	molotov->SetupInitialTransmittedGrenadeVelocity( velocity );
	molotov->SetThrower( owner ); 
	molotov->m_pWeaponInfo = &weaponInfo;
	molotov->SetIsIncGrenade( weaponInfo.m_weaponId == WEAPON_INCGRENADE );

	if ( molotov->m_bIsIncGrenade )
		molotov->SetModel( INCGREN_MODEL );
	else
		molotov->SetModel( MOLOTOV_MODEL );

	molotov->SetGravity( BaseClass::GetGrenadeGravity() );
	molotov->SetFriction( BaseClass::GetGrenadeFriction() );
	molotov->SetElasticity( BaseClass::GetGrenadeElasticity() );

	molotov->SetTouch( &CMolotovProjectile::BounceTouch );
	molotov->SetThink( &CMolotovProjectile::DetonateThink );
	molotov->SetNextThink( gpGlobals->curtime + 2.0f );

	molotov->m_flDamage = 200.0f;
	molotov->m_DmgRadius = 300.0f;
	molotov->ChangeTeam( owner->GetTeamNumber() );
	molotov->ApplyLocalAngularVelocityImpulse( angVelocity );	

	// make NPCs afaid of it while in the air
	molotov->SetThink( &CMolotovProjectile::DangerSoundThink );
	molotov->SetNextThink( gpGlobals->curtime );

	molotov->EmitSound( "Molotov.Throw" );
	molotov->EmitSound( "Molotov.Loop" );
	molotov->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	// we have to reset these here because we set the model late and it resets the collision
	Vector min = Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE );
	Vector max = Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE );
	molotov->SetSize( min, max );
	if ( molotov->CollisionProp() )
		molotov->CollisionProp()->SetCollisionBounds( min, max );

	return molotov;
}

void CMolotovProjectile::InitializeSpawnFromWorld( inputdata_t &inputdata )
{
	SetDetonateTimerLength( molotov_throw_detonate_time.GetFloat() );

	SetGravity( GetGrenadeGravity() );
	SetFriction( GetGrenadeFriction() );
	SetElasticity( GetGrenadeElasticity() );
	SetIsIncGrenade( false );

	SetTouch( &CMolotovProjectile::BounceTouch );
	SetThink( &CMolotovProjectile::DetonateThink );
	SetNextThink( gpGlobals->curtime + 2.0f );

	//pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	ApplyLocalAngularVelocityImpulse( AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ) );

	// make NPCs afaid of it while in the air
	SetThink( &CMolotovProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	EmitSound( "Molotov.Throw" );
	EmitSound( "Molotov.Loop" );
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	// we have to reset these here because we set the model late and it resets the collision
	Vector min = Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE );
	Vector max = Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE );
	SetSize( min, max );
	if ( CollisionProp() )
		CollisionProp()->SetCollisionBounds( min, max );

	m_pWeaponInfo = GetWeaponInfo( WEAPON_MOLOTOV );
}

void CMolotovProjectile::Spawn( void )
{
	m_stillTimer.Invalidate();
	m_throwDetTimer.Invalidate();

	BaseClass::Spawn();

	if ( this->m_bIsIncGrenade )
	{
		SetModel( INCGREN_MODEL );
		SetBodygroupPreset( "thrown" );
	}
	else
	{
		SetModel( MOLOTOV_MODEL );
	}

}

void CMolotovProjectile::Precache( void )
{
	PrecacheModel( MOLOTOV_MODEL );
	PrecacheModel( INCGREN_MODEL );

	PrecacheScriptSound( "Molotov.Throw" );
	PrecacheScriptSound( "Molotov.Loop" );
	PrecacheParticleSystem( "weapon_molotov_thrown" );
	PrecacheParticleSystem( "weapon_molotov_held" );
	PrecacheParticleSystem( "incgrenade_thrown_trail" );

	BaseClass::Precache();
}

void CMolotovProjectile::BounceTouch( CBaseEntity *other )
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
		// only detonate on surfaces less steep than this
		const float kMinCos = cosf(DEG2RAD(weapon_molotov_maxdetonateslope.GetFloat()));
		if ( hitTrace.plane.normal.z >= kMinCos )
		{
			Detonate();
		}
	}
}

void CMolotovProjectile::BounceSound( void )
{
	if ( m_bIsIncGrenade )
		EmitSound( "IncGrenade.Bounce" );
	else
		EmitSound( "GlassBottle.ImpactHard" );
}


void CMolotovProjectile::DetonateThink( void )
{
// 	if( gpGlobals->curtime > m_flDetonateTime )
// 	{
// 		Detonate();
// 		return;
// 	}

	if ( GetAbsVelocity().IsLengthGreaterThan( 5.0f ) )
	{
		m_stillTimer.Invalidate();
	}
	else if ( !m_stillTimer.HasStarted() )
	{
		m_stillTimer.Start();
	}

	const float StillDetonateTime = 0.5f;
	if ( m_stillTimer.HasStarted() && m_stillTimer.GetElapsedTime() > StillDetonateTime )
	{
		Detonate();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	TheBots->SetGrenadeRadius( this, 0.0f );
}


void CMolotovProjectile::Detonate( void )
{
	//BaseClass::Detonate();

	const trace_t &hitTrace = GetTouchTrace();
	if ( hitTrace.surface.flags & SURF_SKY )
		return;

	// tell the bots an HE grenade has exploded
	CCSPlayer *player = ToCSPlayer(GetThrower());
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "molotov_detonate" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	Vector burnPos, splashNormal;

	if ( hitTrace.DidHitWorld() )
	{
		// hit the world, just explode at that position
		burnPos = hitTrace.endpos;
		splashNormal = hitTrace.plane.normal;
	}
	else
	{
		// exploded in the air, or hit an object or player.
		// find the world normal under them (if close enough) and explode there
		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin() + Vector( 0, 0, 10 ), GetAbsOrigin() + Vector( 0, 0, -128.0f ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction == 1 )
		{
			// Too high, just play explosion effect and don't start a fire
			if ( m_bIsIncGrenade )
				EmitSound( "Inferno.Start_IncGrenade" );
			else
				EmitSound( "Inferno.Start" );

			TheBots->SetGrenadeRadius( this, MolotovGrenadeRadius );
			StopSound( "Molotov.Loop" );

			DispatchParticleEffect( "explosion_molotov_air", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

			Vector vecAbsOrigin = GetAbsOrigin();
			CPASFilter filter( vecAbsOrigin );
			te->Explosion( filter, -1.0, // don't apply cl_interp delay
				vecAbsOrigin,
				0,
				32,
				25,
				TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOFIREBALL | TE_EXPLFLAG_NOPARTICLES,
				152,
				50 );

			UTIL_Remove( this );
			return;
		}
		else if( tr.surface.flags & SURF_SKY )
		{
			// just bounce
			return;
		}

		// otherwise explode normally
		burnPos = tr.endpos;
		splashNormal = tr.plane.normal;
	}
	
	TheBots->SetGrenadeRadius( this, MolotovGrenadeRadius );

	CInferno *inferno = (CInferno *)CBaseEntity::Create( "inferno", burnPos, QAngle( 0, 0, 0 ), GetThrower() );
	Vector vBurnDir = m_vInitialVelocity;
	vBurnDir.NormalizeInPlace();
	vBurnDir *= GetAbsVelocity().Length();
	inferno->SetSourceWeaponInfo( m_pWeaponInfo );

	if ( m_bIsIncGrenade )
		inferno->SetInfernoType( INFERNO_TYPE_INCGREN_FIRE );
	else
		inferno->SetInfernoType( INFERNO_TYPE_FIRE );
	inferno->StartBurning( burnPos, splashNormal, vBurnDir );

	if ( inferno->WasCreatedInSmoke() )
	{
		// we extinguished ourselves with this throw.
		m_unOGSExtraFlags |= GRENADE_EXTINGUISHED_INFERNO;	
	}

	// We override the base class detonate and don't chain down-- 
	RecordDetonation();
	
	StopSound( "Molotov.Loop" );
	
	UTIL_Remove( this );
}


#endif
