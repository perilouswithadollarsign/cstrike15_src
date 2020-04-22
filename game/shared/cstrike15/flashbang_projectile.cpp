//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "flashbang_projectile.h"
#include "shake.h"
#include "engine/IEngineSound.h"
#include "cs_player.h"
#include "dlight.h"
#include "keyvalues.h"
#include "weapon_csbase.h"
#include "cs_gamerules.h"
#include "animation.h"

#define GRENADE_MODEL "models/Weapons/w_eq_flashbang_dropped.mdl"


LINK_ENTITY_TO_CLASS( flashbang_projectile, CFlashbangProjectile );
PRECACHE_REGISTER( flashbang_projectile );

#if !defined( CLIENT_DLL )
BEGIN_DATADESC( CFlashbangProjectile )

	// Fields
	//DEFINE_KEYFIELD( m_flTimeToDetonate, FIELD_FLOAT, "TimeToDetonate" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTimer", InputSetTimer ),

END_DATADESC()
#endif

// hack to allow de_nuke vents to occlude flashbangs when closed
class CTraceFilterNoPlayersAndFlashbangPassableAnims : public CTraceFilterNoPlayers
{
public:
	CTraceFilterNoPlayersAndFlashbangPassableAnims( const IHandleEntity *passentity = NULL, int collisionGroup = COLLISION_GROUP_NONE )
		: CTraceFilterNoPlayers( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{

		CBaseEntity *pEnt = EntityFromEntityHandle(pHandleEntity);
		if ( pEnt )
		{
			CBaseAnimating* pAnimating = dynamic_cast< CBaseAnimating* >( pEnt );
			if ( pAnimating )
			{
				// look for the flashbang passable animtag
				float flFlashbangPassable = pAnimating->GetAnySequenceAnimTag( pAnimating->GetSequence(), ANIMTAG_FLASHBANG_PASSABLE, -1 );

				if ( flFlashbangPassable != -1 )
					return false; // model animation is tagged to allow flashbangs through
			}

			// Weapons don't block flashbangs
			CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pEnt );
			CBaseGrenade* pGrenade = dynamic_cast< CBaseGrenade* > ( pEnt );
			if ( pWeapon || pGrenade )
				return false;
		}

		return CTraceFilterNoPlayers::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

float PercentageOfFlashForPlayer(CBaseEntity *player, Vector flashPos, CBaseEntity *pevInflictor)
{
	if (!(player->IsPlayer()))
	{
		// if this entity isn't a player, it's a hostage or some other entity, then don't bother with the expensive checks
		// that come below.
		return 0.0f;
	}

	const float FLASH_FRACTION = 0.167f;
	const float SIDE_OFFSET = 75.0f;

	Vector pos = player->EyePosition();
	Vector vecRight, vecUp;

	QAngle tempAngle;
	VectorAngles(player->EyePosition() - flashPos, tempAngle);
	AngleVectors(tempAngle, NULL, &vecRight, &vecUp);

	vecRight.NormalizeInPlace();
	vecUp.NormalizeInPlace();

	// Set up all the ray stuff.
	// We don't want to let other players block the flash bang so we use this custom filter.
	Ray_t ray;
	trace_t tr;
	CTraceFilterNoPlayersAndFlashbangPassableAnims traceFilter( pevInflictor, COLLISION_GROUP_NONE );
	unsigned int FLASH_MASK = MASK_OPAQUE_AND_NPCS | CONTENTS_DEBRIS;

	// According to comment in IsNoDrawBrush in cmodel.cpp, CONTENTS_OPAQUE is ONLY used for block light surfaces,
	// and we want flashbang traces to pass through those, since the block light surface is only used for blocking
	// lightmap light rays during map compilation.
	FLASH_MASK &= ~CONTENTS_OPAQUE;

	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.m_pEnt == player))
	{
		return 1.0f;
	}

	float retval = 0.0f;

	// check the point straight up.
	pos = flashPos + vecUp*50.0f;
	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );
	// Now shoot it to the player's eye.
	pos = player->EyePosition();
	ray.Init( tr.endpos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.m_pEnt == player))
	{
		retval += FLASH_FRACTION;
	}

	// check the point up and right.
	pos = flashPos + vecRight*SIDE_OFFSET + vecUp*10.0f;
	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );
	// Now shoot it to the player's eye.
	pos = player->EyePosition();
	ray.Init( tr.endpos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.m_pEnt == player))
	{
		retval += FLASH_FRACTION;
	}

	// Check the point up and left.
	pos = flashPos - vecRight*SIDE_OFFSET + vecUp*10.0f;
	ray.Init( flashPos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );
	// Now shoot it to the player's eye.
	pos = player->EyePosition();
	ray.Init( tr.endpos, pos );
	enginetrace->TraceRay( ray, FLASH_MASK, &traceFilter,  &tr );

	if ((tr.fraction == 1.0f) || (tr.m_pEnt == player))
	{
		retval += FLASH_FRACTION;
	}

	return retval;
}

// --------------------------------------------------------------------------------------------------- //
//
// RadiusDamage - this entity is exploding, or otherwise needs to inflict damage upon entities within a certain range.
// 
// only damage ents that can clearly be seen by the explosion!
// --------------------------------------------------------------------------------------------------- //

void RadiusFlash( 
	Vector vecSrc, 
	CBaseEntity *pevInflictor, 
	CBaseEntity *pevAttacker, 
	float flDamage, 
	int iClassIgnore, 
	int bitsDamageType, 
	uint8 *pOutNumOpponentsEffected = NULL, 
	uint8 *pOutNumTeammatesEffected = NULL )
{
	vecSrc.z += 1;// in case grenade is lying on the ground

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	if ( pOutNumOpponentsEffected )
		*pOutNumOpponentsEffected = 0;

	if ( pOutNumTeammatesEffected )
		*pOutNumTeammatesEffected = 0;
	
	trace_t		tr;
	float		flAdjustedDamage;
	variant_t	var;
	Vector		vecEyePos;
	float		fadeTime, fadeHold;
	Vector		vForward;
	Vector		vecLOS;
	float		flDot;
	
	CBaseEntity		*pEntity = NULL;
	static float	flRadius = 3000;
	float			falloff = flDamage / flRadius;

	//bool bInWater = (UTIL_PointContents( vecSrc, MASK_WATER ) == CONTENTS_WATER);

	// iterate on all entities in the vicinity.
	while ((pEntity = gEntList.FindEntityInSphere( pEntity, vecSrc, flRadius )) != NULL)
	{	
		bool bPlayer = pEntity->IsPlayer();
		
		if( !bPlayer )
			continue;

		vecEyePos = pEntity->EyePosition();

		//// blasts used to not travel into or out of water, users assumed it was a bug. Fix is not to run this check -wills
		//if ( bInWater && pEntity->GetWaterLevel() == WL_NotInWater)
		//	continue;
		//if (!bInWater && pEntity->GetWaterLevel() == WL_Eyes)
		//	continue;

		float percentageOfFlash = PercentageOfFlashForPlayer(pEntity, vecSrc, pevInflictor);

		if ( percentageOfFlash > 0.0 )
		{
			if ( pOutNumOpponentsEffected && pEntity->GetTeamNumber() != pevAttacker->GetTeamNumber() )
				(*pOutNumOpponentsEffected)++;
			if ( pOutNumTeammatesEffected && pEntity->GetTeamNumber() == pevAttacker->GetTeamNumber() )
				(*pOutNumTeammatesEffected)++;

			// decrease damage for an ent that's farther from the grenade
			flAdjustedDamage = flDamage - ( vecSrc - pEntity->EyePosition() ).Length() * falloff;

			if ( flAdjustedDamage > 0 )
			{
				// See if we were facing the flash
				AngleVectors( pEntity->EyeAngles(), &vForward );

				vecLOS = ( vecSrc - vecEyePos );

				float flDistance = vecLOS.Length();

				//DebugDrawLine( vecEyePos, vecEyePos + (100.0 * vecLOS), 0, 255, 0, true, 10.0 );
				//DebugDrawLine( vecEyePos, vecEyePos + (100.0 * vForward), 0, 0, 255, true, 10.0 );

				// Normalize both vectors so the dotproduct is in the range -1.0 <= x <= 1.0 
				vecLOS.NormalizeInPlace();


				flDot = DotProduct (vecLOS, vForward);

				float startingAlpha = 255;
	
				// if target is facing the bomb, the effect lasts longer
				if( flDot >= 0.6 )
				{
					// looking at the flashbang
					fadeTime = flAdjustedDamage * 2.5f;
					fadeHold = flAdjustedDamage * 1.25f;
				}
				else if( flDot >= 0.3 )
				{
					// looking to the side
					fadeTime = flAdjustedDamage * 1.75f;
					fadeHold = flAdjustedDamage * 0.8f;
				}
				else if( flDot >= -0.2 )
				{
					// looking to the side
					fadeTime = flAdjustedDamage * 1.00f;
					fadeHold = flAdjustedDamage * 0.5f;
				}
				else
				{
					// facing away
					fadeTime = flAdjustedDamage * 0.5f;
					fadeHold = flAdjustedDamage * 0.25f;
				//	startingAlpha = 200;
				}

				fadeTime *= percentageOfFlash;
				fadeHold *= percentageOfFlash;

				if ( bPlayer )
				{
					// blind players and bots
					CCSPlayer *player = static_cast< CCSPlayer * >( pEntity );

					// [tj] Store who was responsible for the most recent flashbang blinding.
					CCSPlayer *attacker = ToCSPlayer (pevAttacker);
					if ( attacker && player && player->IsAlive() )
					{
						player->SetLastFlashbangAttacker(attacker);

						// score points/penalties for blinding players
						if ( flDot >= 0.0f )
						{
							if ( attacker->GetTeamNumber() == player->GetTeamNumber() )
								CSGameRules()->ScoreBlindFriendly( attacker );
							else
								CSGameRules()->ScoreBlindEnemy( attacker );
						}
					}

					player->Blind( fadeHold, fadeTime, startingAlpha );
					
					// fire an event when a player has been sufficiently blinded as to not
					// be able to perform the training map flashbang range test
					if ( CSGameRules()->IsPlayingTraining() && fadeHold > 1.9f )
					{
						IGameEvent * event = gameeventmanager->CreateEvent( "tr_player_flashbanged" );
						if ( event )
						{
							event->SetInt( "userid", player->GetUserID() );
							gameeventmanager->FireEvent( event );
						}
					}
					
					IGameEvent * event = gameeventmanager->CreateEvent( "player_blind" );
					if ( event )
					{
						event->SetInt( "userid", player->GetUserID() );
						event->SetInt( "attacker", attacker ? attacker->GetUserID() : 0 );
						event->SetInt( "entityid", pevInflictor ? pevInflictor->entindex() : 0 );
						event->SetFloat( "blind_duration", player->m_flFlashDuration ); 
						gameeventmanager->FireEvent( event );
					}

					// deafen players and bots
					player->Deafen( flDistance );
				}
			}	
		}
	}

	CPVSFilter filter(vecSrc);
	te->DynamicLight( filter, 0.0, &vecSrc, 255, 255, 255, 2, 400, 0.1, 768 );
}

// --------------------------------------------------------------------------------------------------- //
// CFlashbangProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CFlashbangProjectile* CFlashbangProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	const CCSWeaponInfo& weaponInfo )
{
	CFlashbangProjectile *pGrenade = (CFlashbangProjectile*)CBaseEntity::Create( "flashbang_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );
	pGrenade->m_pWeaponInfo = &weaponInfo;

	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );

	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

CFlashbangProjectile::CFlashbangProjectile()
{
	m_flDamage = 100;
	// default timer value for when a player throws it
	// can be overridden when spawned from an entity maker
	m_flTimeToDetonate = 1.5;
	m_numOpponentsHit = m_numTeammatesHit = 0;
}

void CFlashbangProjectile::Spawn()
{
	SetModel( GRENADE_MODEL );

	SetDetonateTimerLength( m_flTimeToDetonate );

	SetTouch( &CBaseGrenade::BounceTouch );

	SetThink( &CBaseCSGrenadeProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	SetGravity( BaseClass::GetGrenadeGravity() );
	SetFriction( BaseClass::GetGrenadeFriction() );
	SetElasticity( BaseClass::GetGrenadeElasticity() );

	m_pWeaponInfo = GetWeaponInfo( WEAPON_FLASHBANG );

	BaseClass::Spawn();

	SetBodygroupPreset( "thrown" );
}

void CFlashbangProjectile::Precache()
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Flashbang.Explode" );
	PrecacheScriptSound( "Flashbang.Bounce" );

	BaseClass::Precache();
}

ConVar sv_flashbang_strength( "sv_flashbang_strength", "3.55", FCVAR_REPLICATED, "Flashbang strength", true, 2.0, true, 8.0 );

void CFlashbangProjectile::Detonate()
{
	// tell the bots a flashbang grenade has exploded (and record log events)
	CCSPlayer *player = ToCSPlayer( GetThrower() );
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "flashbang_detonate" );
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

	RadiusFlash ( GetAbsOrigin(), this, GetThrower(), sv_flashbang_strength.GetInt(), CLASS_NONE, DMG_BLAST, &m_numOpponentsHit, &m_numTeammatesHit );
	EmitSound( "Flashbang.Explode" );	

	trace_t		tr;
	Vector		vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 2 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -64 ), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);
	UTIL_DecalTrace( &tr, "Scorch" );

	// Because we don't chain to base, tell ogs to record this detonation here
	RecordDetonation();

	UTIL_Remove( this );
}

//TODO: Let physics handle the sound!
void CFlashbangProjectile::BounceSound( void )
{
	EmitSound( "Flashbang.Bounce" );
}

void CFlashbangProjectile::InputSetTimer( inputdata_t &inputdata )
{
	m_flTimeToDetonate = inputdata.value.Float();
	SetDetonateTimerLength( m_flTimeToDetonate );
}
