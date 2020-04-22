//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basecsgrenade_projectile.h"

extern ConVar sv_gravity;

#ifdef CLIENT_DLL

	#include "c_cs_player.h"
	#include "hltvcamera.h"
	#include "in_buttons.h"
	#include <vgui/IInput.h>
	#include "vgui_controls/Controls.h"

#else

	#include "bot_manager.h"
	#include "cs_player.h"
	#include "soundent.h"
	#include "te_effect_dispatch.h"
	#include "keyvalues.h"
	#include "cs_gamestats.h"
	#include "cs_simple_hostage.h"
	#include "Effects/chicken.h"

	BEGIN_DATADESC( CBaseCSGrenadeProjectile )
	DEFINE_THINKFUNC( DangerSoundThink ),
	END_DATADESC()

	#define GRENADE_FAILSAFE_MAX_BOUNCES 20

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#ifdef CLIENT_DLL
	extern ConVar spec_show_xray;
	extern ConVar sv_grenade_trajectory;
	extern ConVar sv_grenade_trajectory_time_spectator;
	extern ConVar sv_grenade_trajectory_thickness;
	extern ConVar sv_grenade_trajectory_dash;
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )

BEGIN_NETWORK_TABLE( CBaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )
	#ifdef CLIENT_DLL
		RecvPropVector( RECVINFO( m_vInitialVelocity ) ),
		RecvPropInt( RECVINFO( m_nBounces ) )
	#else
		SendPropVector( SENDINFO( m_vInitialVelocity ), 
			20,		// nbits
			0,		// flags
			-3000,	// low value
			3000	// high value
			),
		SendPropInt( SENDINFO( m_nBounces ) )
	#endif
END_NETWORK_TABLE()


#ifdef CLIENT_DLL

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CBaseCSGrenadeProjectile::~CBaseCSGrenadeProjectile()
	{
		flNextTrailLineTime = gpGlobals->curtime;
		CreateGrenadeTrail();
	}

	void CBaseCSGrenadeProjectile::PostDataUpdate( DataUpdateType_t type )
	{
		BaseClass::PostDataUpdate( type );

		//C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( type == DATA_UPDATE_CREATED )
		{
			SetNextClientThink( gpGlobals->curtime );

			vecLastTrailLinePos = GetLocalOrigin();
			flNextTrailLineTime = gpGlobals->curtime + 0.1;

			// Now stick our initial velocity into the interpolation history 
			CInterpolatedVar< Vector > &interpolator = GetOriginInterpolator();
			
			interpolator.ClearHistory();
			float changeTime = GetLastChangeTime( LATCH_SIMULATION_VAR );

			// Add a sample 1 second back.
			Vector vCurOrigin = GetLocalOrigin() - m_vInitialVelocity;
			interpolator.AddToHead( changeTime - 1.0, &vCurOrigin, false );

			// Add the current sample.
			vCurOrigin = GetLocalOrigin();
			interpolator.AddToHead( changeTime, &vCurOrigin, false );


// 			IGameEvent * event = gameeventmanager->CreateEvent( "grenade_projectile_created" );
// 			C_CSPlayer *pPlayer = dynamic_cast<C_CSPlayer*>( GetThrower() );
// 			if( event && pPlayer )
// 			{
// 				event->SetInt( "owneruserid", pPlayer ? pPlayer->GetUserID() : 0 );
// 				event->SetInt( "grenade", GetGrenadeType() );
// 				gameeventmanager->FireEvent( event );
// 			}

			//if ( pLocalPlayer )
			//	pLocalPlayer->NotifyPlayerOfThrownGrenade( this, GetThrower(), GetGrenadeType() );		
		}
		else
		{

		}		
	}

	void CBaseCSGrenadeProjectile::ClientThink( void )
	{
		BaseClass::ClientThink();

		SetNextClientThink( gpGlobals->curtime );

		if ( flNextTrailLineTime <= gpGlobals->curtime )
		{
			CreateGrenadeTrail();
		}
	}

	void CBaseCSGrenadeProjectile::CreateGrenadeTrail( void )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( !pLocalPlayer )
			return;

		if ( flNextTrailLineTime <= gpGlobals->curtime )
		{
			bool bRenderForSpectator = CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt();
			if ( sv_grenade_trajectory_time_spectator.GetFloat() > 0.0f && sv_grenade_trajectory.GetInt() == 0 && pLocalPlayer && ( bRenderForSpectator || ( !pLocalPlayer->IsAlive() && ( pLocalPlayer->GetObserverMode() > OBS_MODE_FREEZECAM ) ) ) )
			{
				bool bRender = false;

				if ( ( GetTeamNumber() == TEAM_CT ) && ( bRenderForSpectator || ( pLocalPlayer->GetTeamNumber() == TEAM_CT ) ) )
					bRender = true;
				else if ( ( GetTeamNumber() == TEAM_TERRORIST ) && ( bRenderForSpectator || ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST ) ) )
					bRender = true;

				if ( bRender )
				{
					// Grenade trails for spectators
					//CInterpolatedVar< Vector > &interpolator = GetOriginInterpolator();

					QAngle angGrTrajAngles;
					Vector vec3tempOrientation = ( vecLastTrailLinePos - GetLocalOrigin() );
					VectorAngles( vec3tempOrientation, angGrTrajAngles );

					float flGrTraThickness = sv_grenade_trajectory_thickness.GetFloat();
					Vector vec3_GrTrajMin = Vector( 0, -flGrTraThickness, -flGrTraThickness );
					Vector vec3_GrTrajMax = Vector( vec3tempOrientation.Length(), flGrTraThickness, flGrTraThickness );
					bool bDotted = ( sv_grenade_trajectory_dash.GetInt() && ( fmod( gpGlobals->curtime, 0.1f ) < 0.05f ) );

					Color traceColor;
					if ( GetTeamNumber() == TEAM_CT )
					{
						traceColor[0] = 114;
						traceColor[1] = 155;
						traceColor[2] = 221;
					}
					else
					{
						traceColor[0] = 224;
						traceColor[1] = 175;
						traceColor[2] = 86;
					}

					if ( bDotted )
					{
						traceColor[0] /= 10;
						traceColor[1] /= 10;
						traceColor[2] /= 10;
					}

					//Add extruded box shapes to glow pass to build the arc
					GlowObjectManager().AddGlowBox( GetLocalOrigin(), angGrTrajAngles, vec3_GrTrajMin, vec3_GrTrajMax, traceColor, sv_grenade_trajectory_time_spectator.GetFloat() );

					//Make the grenade projectile itself glow
					if ( !m_GlowObject.IsRendering() )
					{
						m_GlowObject.SetColor( Vector( traceColor[0] / 255.0f, traceColor[1] / 255.0f, traceColor[2] / 255.0f ) );
						m_GlowObject.SetAlpha( 0.3f );
						m_GlowObject.SetGlowAlphaCappedByRenderAlpha( true );
						m_GlowObject.SetGlowAlphaFunctionOfMaxVelocity( 50.0f );
						m_GlowObject.SetGlowAlphaMax( 0.3f );
						m_GlowObject.SetRenderFlags( true, true );
					}
				}
			}

			vecLastTrailLinePos = GetLocalOrigin();
			flNextTrailLineTime = gpGlobals->curtime + 0.05;
		}
	}

	int CBaseCSGrenadeProjectile::DrawModel( int flags, const RenderableInstance_t &instance )
	{
		// During the first half-second of our life, don't draw ourselves if he's
		// still playing his throw animation.
		// (better yet, we could draw ourselves in his hand).
		if ( GetThrower() != C_BasePlayer::GetLocalPlayer() )
		{
			if ( gpGlobals->curtime - m_flSpawnTime < 0.5 )
			{
				C_CSPlayer *pPlayer = dynamic_cast<C_CSPlayer*>( GetThrower() );
				if ( pPlayer && pPlayer->m_PlayerAnimState->IsThrowingGrenade() )
				{
					return 0;
				}
			}
		}

		return BaseClass::DrawModel( flags, instance );
	}

	void CBaseCSGrenadeProjectile::Spawn()
	{
		m_flSpawnTime = gpGlobals->curtime;
		BaseClass::Spawn();
	}

#else

	void CBaseCSGrenadeProjectile::PostConstructor( const char *className )
	{
		BaseClass::PostConstructor( className );
		TheBots->AddGrenade( this );
	}

	CBaseCSGrenadeProjectile::~CBaseCSGrenadeProjectile()
	{	
		TheBots->RemoveGrenade( this );
	}

	void CBaseCSGrenadeProjectile::Precache()
	{
		BaseClass::Precache();
		PrecacheEffect( "gunshotsplash" );
	}

	void CBaseCSGrenadeProjectile::Spawn( void )
	{
		Precache();
		BaseClass::Spawn();

		SetSolidFlags( FSOLID_NOT_STANDABLE );
		SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
		SetSolid( SOLID_BBOX );	// So it will collide with physics props!
		AddFlag( FL_GRENADE );

		m_lastHitPlayer = NULL;

		// smaller, cube bounding box so we rest on the ground
		Vector min = Vector( -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE, -GRENADE_DEFAULT_SIZE );
		Vector max = Vector( GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE, GRENADE_DEFAULT_SIZE );

		SetSize( min, max );
 		if ( CollisionProp( ) )
 			CollisionProp( )->SetCollisionBounds( min, max );

		m_nBounces = 0;
	}

	int	CBaseCSGrenadeProjectile::UpdateTransmitState()
	{
		// always call ShouldTransmit() for grenades
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}

	int CBaseCSGrenadeProjectile::ShouldTransmit( const CCheckTransmitInfo *pInfo )
	{
		CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
		if ( pRecipientEntity->IsPlayer() )
		{
			CBasePlayer *pRecipientPlayer = static_cast<CBasePlayer*>( pRecipientEntity );

			// always transmit to the thrower of the grenade
			if ( pRecipientPlayer && ( (GetThrower() && pRecipientPlayer == GetThrower()) ||
				pRecipientPlayer->GetTeamNumber() == TEAM_SPECTATOR) )
			{
				return FL_EDICT_ALWAYS;
			}
		}

		return FL_EDICT_PVSCHECK;
	}

	void CBaseCSGrenadeProjectile::DangerSoundThink( void )
	{
		if (!IsInWorld())
		{
			Remove( );
			return;
		}

		if( gpGlobals->curtime > m_flDetonateTime )
		{
			Detonate();
			return;
		}

		CSoundEnt::InsertSound ( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, GetAbsVelocity().Length( ), 0.2 );

		SetNextThink( gpGlobals->curtime + 0.2 );

		if (GetWaterLevel() != WL_NotInWater)
		{
			SetAbsVelocity( GetAbsVelocity() * 0.5 );
		}
	}


	//Sets the time at which the grenade will explode
	void CBaseCSGrenadeProjectile::SetDetonateTimerLength( float timer )
	{
		m_flDetonateTime = gpGlobals->curtime + timer;
	}

	unsigned int CBaseCSGrenadeProjectile::PhysicsSolidMaskForEntity( void ) const
	{
		if ( GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
		{
			return ((CONTENTS_GRENADECLIP | MASK_SOLID) & ~CONTENTS_MONSTER);
		}
		else
		{
			return (CONTENTS_GRENADECLIP|MASK_SOLID|MASK_VISIBLE_AND_NPCS|CONTENTS_HITBOX) & ~(CONTENTS_DEBRIS);
		}
	}

	void CBaseCSGrenadeProjectile::ResolveFlyCollisionCustom( trace_t &trace, Vector &vecMove )
	{
		const float kSleepVelocity = 20.0f;
		const float kSleepVelocitySquared = kSleepVelocity * kSleepVelocity; 

		// Verify that we have an entity.
		CBaseEntity *pEntity = trace.m_pEnt;
		Assert( pEntity );
		
		if ( pEntity )
		{
			CChicken *pChicken = dynamic_cast< CChicken* >( pEntity );
			if (pChicken)
			{
				// hurt the chicken
				CTakeDamageInfo info( this, this, 10, DMG_CLUB );
				pChicken->DispatchTraceAttack( info, GetAbsVelocity().Normalized(), &trace );
				ApplyMultiDamage();

				return;
			}
		}

		// if its breakable glass and we kill it, don't bounce.
		// give some damage to the glass, and if it breaks, pass 
		// through it.
		bool breakthrough = false;

		if( pEntity && FClassnameIs( pEntity, "func_breakable" ) )
		{
			breakthrough = true;
		}

		if( pEntity && FClassnameIs( pEntity, "func_breakable_surf" ) )
		{
			breakthrough = true;
		}

		if( pEntity && FClassnameIs( pEntity, "prop_physics_multiplayer" ) && pEntity->GetMaxHealth() > 0 && pEntity->m_takedamage == DAMAGE_YES )
		{
			breakthrough = true;
		}

		// this one is tricky because BounceTouch hits breakable propers before we hit this function and the damage is already applied there (CBaseGrenade::BounceTouch( CBaseEntity *pOther ))
		// by the time we hit this, the prop hasn't been removed yet, but it broke, is set to not take anymore damage and is marked for deletion - we have to cover this case here
		if( pEntity && FClassnameIs( pEntity, "prop_dynamic" ) && pEntity->GetMaxHealth() > 0 && (pEntity->m_takedamage == DAMAGE_YES || (pEntity->m_takedamage == DAMAGE_NO && pEntity->IsEFlagSet( EFL_KILLME ))) )
		{
			breakthrough = true;
		}

		if ( breakthrough )
		{
			CTakeDamageInfo info( this, this, 10, DMG_CLUB );
			pEntity->DispatchTraceAttack( info, GetAbsVelocity().Normalized(), &trace );

			ApplyMultiDamage();

			if( pEntity->m_iHealth <= 0 )
			{
				// slow our flight a little bit
				Vector vel = GetAbsVelocity();

				vel *= 0.4;

				SetAbsVelocity( vel );
				return;
			}
		}


		//Assume all surfaces have the same elasticity
		float flSurfaceElasticity = 1.0;

		//Don't bounce off of players with perfect elasticity
		if ( pEntity && pEntity->IsPlayer() )
		{
			flSurfaceElasticity = 0.3f;

			// and do slight damage to players on the opposite team
			if ( GetTeamNumber() != pEntity->GetTeamNumber() )
			{
				CTakeDamageInfo info( this, GetThrower(), 2, DMG_GENERIC );

				pEntity->TakeDamage( info );
			}
		}

		//Don't bounce twice on a selection of problematic entities
		bool bIsProjectile = dynamic_cast< CBaseCSGrenadeProjectile* >( pEntity ) != NULL;
		if ( pEntity && !pEntity->IsWorld() && m_lastHitPlayer.Get() == pEntity )
		{
			bool bIsHostage = dynamic_cast< CHostage* >( pEntity ) != NULL;
			if (  pEntity->IsPlayer() || bIsHostage || bIsProjectile )
			{
				//DevMsg( "Setting %s to DEBRIS, it is in group %i, it hit %s in group %i\n", this->GetClassname(), this->GetCollisionGroup(), pEntity->GetClassname(), pEntity->GetCollisionGroup() );
				SetCollisionGroup( COLLISION_GROUP_DEBRIS );
				if ( bIsProjectile )
				{
					//DevMsg( "Setting %s to DEBRIS, it is in group %i.\n", pEntity->GetClassname(), pEntity->GetCollisionGroup() );
					pEntity->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
				}
				return;
			}
		}
		if ( pEntity )
		{
			m_lastHitPlayer = pEntity;
		}

		float flTotalElasticity = GetElasticity() * flSurfaceElasticity;
		flTotalElasticity = clamp( flTotalElasticity, 0.0f, 0.9f );

		// NOTE: A backoff of 2.0f is a reflection
		Vector vecAbsVelocity;
		PhysicsClipVelocity( GetAbsVelocity(), trace.plane.normal, vecAbsVelocity, 2.0f );
		vecAbsVelocity *= flTotalElasticity;

		// Get the total velocity (player + conveyors, etc.)
		VectorAdd( vecAbsVelocity, GetBaseVelocity(), vecMove );
		float flSpeedSqr = DotProduct( vecMove, vecMove );

		bool bIsWeapon = dynamic_cast< CBaseCombatWeapon* >( pEntity ) != NULL;
		
		// Stop if on ground or if we bounce and our velocity is really low (keeps it from bouncing infinitely)
		if ( pEntity &&
			( ( trace.plane.normal.z > 0.7f ) || (trace.plane.normal.z > 0.1f && flSpeedSqr < kSleepVelocitySquared) ) &&
			( pEntity->IsStandable() || bIsProjectile || bIsWeapon || pEntity->IsWorld() ) 
			)
		{
			// clip it again to emulate old behavior and keep it from bouncing up like crazy when you throw it at the ground on the first toss
			if ( flSpeedSqr > 96000 )
			{
				float alongDist = DotProduct( vecAbsVelocity.Normalized(), trace.plane.normal );
				if ( alongDist > 0.5f )
				{
					float flBouncePadding = (1.0f - alongDist) + 0.5f;
					vecAbsVelocity *= flBouncePadding;
				}
			}

			SetAbsVelocity( vecAbsVelocity );

			if ( flSpeedSqr < kSleepVelocitySquared )
			{
				SetGroundEntity( pEntity );

				// Reset velocities.
				SetAbsVelocity( vec3_origin );
				SetLocalAngularVelocity( vec3_angle );

				//align to the ground so we're not standing on end
				QAngle angle;
				VectorAngles( trace.plane.normal, angle );

				// rotate randomly in yaw
				angle[1] = random->RandomFloat( 0, 360 );

				// TODO: rotate around trace.plane.normal
				
				SetAbsAngles( angle );			
			}
			else
			{
				Vector vecBaseDir = GetBaseVelocity();
				if ( !vecBaseDir.IsZero() )
				{
					VectorNormalize( vecBaseDir );
					Vector vecDelta = GetBaseVelocity() - vecAbsVelocity;	
					float flScale = vecDelta.Dot( vecBaseDir );
					vecAbsVelocity += GetBaseVelocity() * flScale;
				}
					
				VectorScale( vecAbsVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, vecMove ); 
				
				PhysicsPushEntity( vecMove, &trace );
			}
		}
		else
		{
			SetAbsVelocity( vecAbsVelocity );
			VectorScale( vecAbsVelocity, ( 1.0f - trace.fraction ) * gpGlobals->frametime, vecMove ); 
			PhysicsPushEntity( vecMove, &trace );
		}
		
		BounceSound();

		// tell the bots a grenade has bounced
		CCSPlayer *player = ToCSPlayer(GetThrower());
		if ( player )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "grenade_bounce" );
			if ( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				event->SetFloat( "x", GetAbsOrigin().x );
				event->SetFloat( "y", GetAbsOrigin().y );
				event->SetFloat( "z", GetAbsOrigin().z );
				gameeventmanager->FireEvent( event );
			}
		}

		OnBounced();

		if (m_nBounces > GRENADE_FAILSAFE_MAX_BOUNCES )
		{
			//failsafe detonate after 20 bounces
			SetAbsVelocity( vec3_origin );
			DetonateOnNextThink();
			SetNextThink( gpGlobals->curtime );
			SetMoveType( MOVETYPE_NONE );
		}
		else
		{
			m_nBounces++;
		}
		

	}

	void CBaseCSGrenadeProjectile::SetupInitialTransmittedGrenadeVelocity( const Vector &velocity )
	{
		m_vInitialVelocity = velocity;
	}

	#define	MAX_WATER_SURFACE_DISTANCE	512

	void CBaseCSGrenadeProjectile::Splash()
	{
		Vector centerPoint = GetAbsOrigin();
		Vector normal( 0, 0, 1 );

		// Find our water surface by tracing up till we're out of the water
		trace_t tr;
		Vector vecTrace( 0, 0, MAX_WATER_SURFACE_DISTANCE );
		UTIL_TraceLine( centerPoint, centerPoint + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );

		// If we didn't start in water, we're above it
		if ( tr.startsolid == false )
		{
			// Look downward to find the surface
			vecTrace.Init( 0, 0, -MAX_WATER_SURFACE_DISTANCE );
			UTIL_TraceLine( centerPoint, centerPoint + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );

			// If we hit it, setup the explosion
			if ( tr.fraction < 1.0f )
			{
				centerPoint = tr.endpos;
			}
			else
			{
				//NOTENOTE: We somehow got into a splash without being near water?
				Assert( 0 );
			}
		}
		else if ( tr.fractionleftsolid )
		{
			// Otherwise we came out of the water at this point
			centerPoint = centerPoint + (vecTrace * tr.fractionleftsolid);
		}
		else
		{
			// Use default values, we're really deep
		}

		CEffectData	data;
 		data.m_vOrigin = centerPoint;
		data.m_vNormal = normal;
		data.m_flScale = random->RandomFloat( 1.0f, 2.0f );

		if ( GetWaterType() & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "gunshotsplash", data );
	}

	// Add a row to ogs for the explosion of this grenade. Damage instances are recorded separately.
	void CBaseCSGrenadeProjectile::RecordDetonation( void )
	{
		// I hate having to call this from so many places since half the grenades override the standard detonate and the rest dont...
		// If this triggers, it's because either somebody changed the way some grenade code chains to base methods or it's taking an uncommon code path
		// that needs to be investigated.
		if ( m_bDetonationRecorded )
		{
			Warning( "Detonation of grenade '%s' attempted to record twice!\n", GetDebugName() );
			Assert( 0 );
			return;
		}

		CCSPlayer::StartNewBulletGroup();

		SWeaponHitData *pHitData = new SWeaponHitData;
		if ( pHitData->InitAsGrenadeDetonation( this, CCSPlayer::GetBulletGroup() ) )
		{
			CCS_GameStats.RecordWeaponHit( pHitData ); // submission deletes the struct.
			m_bDetonationRecorded = true;
		}
		else
		{
			delete pHitData;
		}
	}

	void CBaseCSGrenadeProjectile::Explode( trace_t *pTrace, int bitsDamageType )
	{
		RecordDetonation();
		BaseClass::Explode( pTrace, bitsDamageType );
	}

#endif // !CLIENT_DLL
