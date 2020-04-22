//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "smokegrenade_projectile.h"
#include "weapon_csbase.h"
#include "particle_parse.h"
#if defined( CLIENT_DLL )
#include "c_cs_player.h"
#else
#include "sendproxy.h"
#include "particle_smokegrenade.h"
#include "cs_player.h"
#include "keyvalues.h"
#include "bot_manager.h"
#include "Effects/inferno.h"
#endif

#define GRENADE_MODEL "models/Weapons/w_eq_smokegrenade_thrown.mdl"


#if defined( CLIENT_DLL )

IMPLEMENT_CLIENTCLASS_DT( C_SmokeGrenadeProjectile, DT_SmokeGrenadeProjectile, CSmokeGrenadeProjectile )
RecvPropBool( RECVINFO( m_bDidSmokeEffect ) ),
RecvPropInt( RECVINFO( m_nSmokeEffectTickBegin ) )
END_RECV_TABLE()

C_SmokeGrenadeProjectile::~C_SmokeGrenadeProjectile()
{
	RemoveSmokeGrenadeHandle( this );
}

void C_SmokeGrenadeProjectile::PostDataUpdate( DataUpdateType_t type )
{
	BaseClass::PostDataUpdate( type );	
}

void C_SmokeGrenadeProjectile::OnDataChanged( DataUpdateType_t updateType ) 
{ 
	if ( ( m_nSmokeEffectTickBegin || m_bDidSmokeEffect ) && !m_bSmokeEffectSpawned )
	{
		SpawnSmokeEffect();
		// And the smoke grenade particle began! - every call but the first is extraneous here
		AddSmokeGrenadeHandle( this );
	}
}

void C_SmokeGrenadeProjectile::SpawnSmokeEffect( )
{
	if ( !m_bSmokeEffectSpawned )
	{
		m_bSmokeEffectSpawned = true;
		CNewParticleEffect *pSmokeEffect = NULL;

		// Used to be: 
		int nUseMethod = 2;
		if ( nUseMethod == 0 )
		{
			// this is the closest to the old method; it doesn't let us correct the lifetime of the particle system in case of full frame update
			DispatchParticleEffect( "explosion_smokegrenade", GetAbsOrigin(), QAngle( 0, 0, 0 ) );// note QAngle(0,0,0). But we need to simulate the particle effect forward sometimes, so we need to use different API now.
		}
		else
		{
			Vector vOrigin = GetNetworkOrigin();
			if ( nUseMethod == 1 )
			{
				// This method works, but isn't the closest to the old method. The old method used CNewParticleEffect::CreateOrAggregate() API in its guts, but aggregation is implicitly disabled by explosion_smokegrenade particle definition as of Dec 2015 in CSGO staging.
				pSmokeEffect = ParticleProp()->Create( "explosion_smokegrenade", PATTACH_CUSTOMORIGIN );
			}
			else
			{
				// The old method used CNewParticleEffect::CreateOrAggregate() API in its guts, so this is the closest method to create smoke to the old method, but it's not been tested in trunk
				pSmokeEffect = CNewParticleEffect::CreateOrAggregate( NULL, "explosion_smokegrenade", vOrigin );
			}

			if ( pSmokeEffect )
			{
				pSmokeEffect->SetSortOrigin( vOrigin );
				pSmokeEffect->SetControlPoint( 0, vOrigin );
				pSmokeEffect->SetControlPoint( 1, vOrigin );
				pSmokeEffect->SetControlPointOrientation( 0, Vector( 1, 0, 0 ), Vector( 0, -1, 0 ), Vector( 0, 0, 1 ) );
			}
		}

		if ( m_nSmokeEffectTickBegin )
		{
			int nSkipFrames = gpGlobals->tickcount - m_nSmokeEffectTickBegin;
			if ( nSkipFrames > 4 && pSmokeEffect )
			{
				//Note: pSmokeEffect->Simulate( flSkipSeconds ); would be ideal, but it doesn't work well for long intervals. SkipToTime would be even better but it will extinguish the particle effect if it skips past 2 seconds due to some perf heuristic, and it's not clear if it skips correctly either.
				// this doesn't happen often, and when it does, it's on connection or on replay begin/end, so a little hitch shouldn't be a problem.
				for ( int i = 2; i < nSkipFrames; i += 2 )
					pSmokeEffect->Simulate( gpGlobals->interval_per_tick * 2 );
			}
		}
	}
}

#else // GAME_DLL
LINK_ENTITY_TO_CLASS( smokegrenade_projectile, CSmokeGrenadeProjectile );
PRECACHE_REGISTER( smokegrenade_projectile );

IMPLEMENT_SERVERCLASS_ST( CSmokeGrenadeProjectile, DT_SmokeGrenadeProjectile )
SendPropBool( SENDINFO( m_bDidSmokeEffect ) ),
SendPropInt( SENDINFO( m_nSmokeEffectTickBegin ) )
END_SEND_TABLE()

BEGIN_DATADESC( CSmokeGrenadeProjectile )
	DEFINE_THINKFUNC( Think_Detonate ),
	DEFINE_THINKFUNC( Think_Fade ),
	DEFINE_THINKFUNC( Think_Remove )
END_DATADESC()

CSmokeGrenadeProjectile* CSmokeGrenadeProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CSmokeGrenadeProjectile *pGrenade = (CSmokeGrenadeProjectile*)CBaseEntity::Create( "smokegrenade_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetTimer( 1.5 );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );
	pGrenade->SetGravity( 0.55 );
	pGrenade->SetFriction( 0.7 );
	pGrenade->m_flDamage = 100;
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );	
	pGrenade->SetTouch( &CBaseGrenade::BounceTouch );

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );
	pGrenade->m_bDidSmokeEffect = false;
	pGrenade->m_nSmokeEffectTickBegin = 0;
	pGrenade->m_flLastBounce = 0;
	pGrenade->m_vSmokeColor = weaponInfo.GetSmokeColor();
	pGrenade->m_pWeaponInfo = &weaponInfo;

	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}


void CSmokeGrenadeProjectile::SetTimer( float timer )
{
	SetThink( &CSmokeGrenadeProjectile::Think_Detonate );
	SetNextThink( gpGlobals->curtime + timer );

	TheBots->SetGrenadeRadius( this, 0.0f );
}

void CSmokeGrenadeProjectile::Think_Detonate()
{
	if ( GetAbsVelocity().Length() > 0.1 )
	{
		// Still moving. Don't detonate yet.
		SetNextThink( gpGlobals->curtime + 0.2 );
		return;
	}

	SmokeDetonate();
}

void CSmokeGrenadeProjectile::SmokeDetonate( void )
{
	TheBots->SetGrenadeRadius( this, SmokeGrenadeRadius );

	// Ok, we've stopped rolling or whatever. Now detonate.

	// Make sure all players get the message about this smoke effect.
	// This fixes an exploit where a player could enter a room where others were seeing smoke and he wasn't
	// because he wasn't in the PVS when the smoke effect started.
	m_nSmokeEffectTickBegin = gpGlobals->tickcount; // client will star the explosion_smokegrenade particle effect at AbsOrigin

	//tell the hostages about the smoke!
	CBaseEntity *pEntity = NULL;
	variant_t var;	//send the location of the smoke?
	var.SetVector3D( GetAbsOrigin() );
	while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "hostage_entity" ) ) != NULL)
	{
		//send to hostages that have a resonable chance of being in it while its still smoking
		if( (GetAbsOrigin() - pEntity->GetAbsOrigin()).Length() < 1000 )
			pEntity->AcceptInput( "smokegrenade", this, this, var, 0 );
	}

	// tell the bots a smoke grenade has exploded
	CCSPlayer *player = ToCSPlayer(GetThrower());
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "smokegrenade_detonate" );
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

	// We avoid our base class detonation, so add the ogs record of our explsion here.
	// Note: this has to be after the gameevent is fired and serviced by server-side listeners
	// so we can run the 'smoke grenade extinguishing infernos' logic and have ogs relevant state set from that.
	RecordDetonation();

	m_bDidSmokeEffect = true; //<- the old way to signal the start of smoke effect; the new way is to set the particle start tick, so that we can replay and fix the bug when we lose the smoke effect when we connect right after smoke grenade went off

	EmitSound( "BaseSmokeEffect.Sound" );

	m_nRenderMode = kRenderTransColor;

	SetMoveType(MOVETYPE_NONE);
	SetNextThink( gpGlobals->curtime + 12.5f );
	SetThink( &CSmokeGrenadeProjectile::Think_Fade );
	
	SetSolid( SOLID_NONE );
}

void CSmokeGrenadeProjectile::RemoveGrenadeFromLists( void )
{
	TheBots->RemoveGrenade( this );
	SetModelName( NULL_STRING );//invisible
	SetSolid( SOLID_NONE );

	CCSPlayer *player = ToCSPlayer(GetThrower());
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "smokegrenade_expired" );
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

}


// Fade the projectile out over time before making it disappear
void CSmokeGrenadeProjectile::Think_Fade()
{
	SetNextThink( gpGlobals->curtime );

	byte a = GetRenderAlpha();
	a -= 1;
	SetRenderAlpha( a );

	if ( !a )
	{
		//RemoveGrenadeFromLists();
		SetNextThink( gpGlobals->curtime + 1.0 );
		SetThink( &CSmokeGrenadeProjectile::Think_Remove );	// Spit out smoke for 10 seconds.
	}
}


void CSmokeGrenadeProjectile::Think_Remove()
{
	RemoveGrenadeFromLists();
	SetMoveType( MOVETYPE_NONE );
	UTIL_Remove( this );
}

//Implement this so we never call the base class,
//but this should never be called either.
void CSmokeGrenadeProjectile::Detonate( void )
{
	Assert(!"Smoke grenade handles its own detonation");
}


void CSmokeGrenadeProjectile::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();

	SetBodygroupPreset( "thrown" );
}


void CSmokeGrenadeProjectile::Precache()
{
	PrecacheModel( GRENADE_MODEL );
	PrecacheScriptSound( "BaseSmokeEffect.Sound" );
	PrecacheScriptSound( "SmokeGrenade.Bounce" );
	BaseClass::Precache();
}


void CSmokeGrenadeProjectile::OnBounced( void )
{
	if ( m_flLastBounce >= ( gpGlobals->curtime - 3*gpGlobals->interval_per_tick ) )
		return;

	m_flLastBounce = gpGlobals->curtime;

	//
	// if the smoke grenade is above ground, trace down to the ground and see where it would end up?
	//
	Vector posDropSmoke = GetAbsOrigin();
	trace_t trSmokeTrace;
	UTIL_TraceLine( posDropSmoke, posDropSmoke - Vector( 0, 0, SmokeGrenadeRadius ), ( MASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP ),
		this, COLLISION_GROUP_PROJECTILE, &trSmokeTrace );
	if ( !trSmokeTrace.startsolid )
	{
		if ( trSmokeTrace.fraction >= 1.0f )
			return;	// this smoke cannot drop enough to cause extinguish

		if ( trSmokeTrace.fraction > 0.001f )
			posDropSmoke = trSmokeTrace.endpos;
	}

	//
	// See if it touches any inferno?
	//
	const int maxEnts = 64;
	CBaseEntity *list[ maxEnts ];
	int count = UTIL_EntitiesInSphere( list, maxEnts, GetAbsOrigin(), 512, FL_ONFIRE );
	for( int i=0; i<count; ++i )
	{
		if (list[i] == NULL || list[i] == this)
			continue;

		CInferno* pInferno = dynamic_cast<CInferno*>( list[i] );

		if ( pInferno && pInferno->BShouldExtinguishSmokeGrenadeBounce( this, posDropSmoke ) )
		{
			if ( posDropSmoke != GetAbsOrigin() )
			{
				const QAngle qAngOriginZero = vec3_angle;
				const Vector vVelocityZero = vec3_origin;
				Teleport( &posDropSmoke, &qAngOriginZero, &vVelocityZero );
			}

			SmokeDetonate();
			break;
		}
	}
}


void CSmokeGrenadeProjectile::BounceSound( void )
{
	if ( !m_bDidSmokeEffect )
	{
		EmitSound( "SmokeGrenade.Bounce" );
	}
}

#endif