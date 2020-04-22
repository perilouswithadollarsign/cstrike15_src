//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "basecombatweapon_shared.h"
#include "baseviewmodel_shared.h"
#include "particles_new.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar r_drawtracers( "r_drawtracers", "1", FCVAR_CHEAT );
ConVar r_drawtracers_firstperson( "r_drawtracers_firstperson", "1", FCVAR_ARCHIVE | FCVAR_RELEASE, "Toggle visibility of first person weapon tracers" );
ConVar r_drawtracers_movetonotintersect( "r_drawtracers_movetonotintersect", "1", FCVAR_CHEAT, "" );
extern ConVar cl_righthand;
void FormatViewModelAttachment( C_BasePlayer *pPlayer, Vector &vOrigin, bool bInverse );

#define	TRACER_SPEED			5000 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector GetTracerOrigin( const CEffectData &data )
{
	Vector vecStart = data.m_vStart;
	QAngle vecAngles;

	int iAttachment = data.m_nAttachmentIndex;;

	// Attachment?
	if ( data.m_fFlags & TRACER_FLAG_USEATTACHMENT )
	{
		C_BaseViewModel *pViewModel = NULL;

		// If the entity specified is a weapon being carried by this player, use the viewmodel instead
		IClientRenderable *pRenderable = data.GetRenderable();
		if ( !pRenderable )
			return vecStart;

		C_BaseEntity *pEnt = data.GetEntity();

// This check should probably be for all multiplayer games, investigate later
#if defined( TF_CLIENT_DLL )
		if ( pEnt && pEnt->IsDormant() )
			return vecStart;
#endif

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );		

			C_BaseCombatWeapon *pWpn = ToBaseCombatWeapon( pEnt );
			if ( pWpn && pWpn->IsCarriedByLocalPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( pWpn->GetOwner() );

				pViewModel = player ? player->GetViewModel( 0 ) : NULL;
				if ( pViewModel )
				{
					// Get the viewmodel and use it instead
					pRenderable = pViewModel;
					break;
				}
			}
		}

		// Get the attachment origin
		if ( !pRenderable->GetAttachment( iAttachment, vecStart, vecAngles ) )
		{
			DevMsg( "GetTracerOrigin: Couldn't find attachment %d on model %s\n", iAttachment, 
				modelinfo->GetModelName( pRenderable->GetModel() ) );
		}
	}

	return vecStart;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TracerCallback( const CEffectData &data )
{
	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	if ( !r_drawtracers.GetBool() )
		return;

	if ( !r_drawtracers_firstperson.GetBool() )
	{
		C_BaseViewModel *pViewModel = ToBaseViewModel(data.GetEntity());
		if ( pViewModel )
			return;
	}

	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	float flVelocity = data.m_flScale;
	bool bWhiz = (data.m_fFlags & TRACER_FLAG_WHIZ);
	int iEntIndex = data.entindex();

	if ( IsPlayerIndex( iEntIndex ) && C_BasePlayer::IsLocalPlayer( C_BaseEntity::Instance( iEntIndex ) ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( C_BaseEntity::Instance( iEntIndex ) );
		Vector	foo = data.m_vStart;
		QAngle	vangles;
		Vector	vforward, vright, vup;

		engine->GetViewAngles( vangles );
		AngleVectors( vangles, &vforward, &vright, &vup );

		VectorMA( data.m_vStart, 4, vright, foo );
		foo[2] -= 0.5f;

		FX_PlayerTracer( foo, (Vector&)data.m_vOrigin );
		return;
	}
	
	// Use default velocity if none specified
	if ( !flVelocity )
	{
		flVelocity = TRACER_SPEED;
	}

	// Do tracer effect
	FX_Tracer( (Vector&)vecStart, (Vector&)data.m_vOrigin, flVelocity, bWhiz );
}

DECLARE_CLIENT_EFFECT_BEGIN( Tracer, TracerCallback )
	PRECACHE( MATERIAL, "effects/spark" )
	PRECACHE( GAMESOUND, "Bullets.DefaultNearmiss" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static int s_nWeaponTracerIndex;

void ParticleTracerCallback( const CEffectData &data )
{
	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	if ( !r_drawtracers.GetBool() )
		return;

	C_BaseEntity *pEntity = data.GetEntity();
	C_BasePlayer *pPlayer = NULL;
	C_BaseViewModel *pViewModel = ToBaseViewModel(pEntity);
	if ( pEntity )
	{
		pPlayer = dynamic_cast<C_BasePlayer*>( pEntity );
		if ( pPlayer && !pViewModel )
			pViewModel = pPlayer->GetViewModel( 0 );

		if ( !r_drawtracers_firstperson.GetBool() && pPlayer )
		{	
			if ( pViewModel )
				return;
		}
	}

	int nParticleIndex = (data.m_nHitBox > 1) ? data.m_nHitBox : s_nWeaponTracerIndex;
	Vector vecEnd;
	// Grab the data
	
	C_BaseCombatWeapon *pWpn = (pEntity) ? pEntity->MyCombatWeaponPointer() : NULL;
	if ( (!pWpn && !pViewModel) || !(data.m_fFlags & TRACER_FLAG_USEATTACHMENT) )
	{
		Vector vecStart = GetTracerOrigin( data );
		vecEnd = data.m_vOrigin;

		// if the tracer visually hits anything that it should not, we move the tracer to almost match the bullet trace itself
		if ( r_drawtracers_movetonotintersect.GetBool() )
		{
			trace_t tr;
			UTIL_TraceLine( vecStart, vecEnd, MASK_SHOT, pEntity, COLLISION_GROUP_PROJECTILE, &tr );
			if ( tr.fraction != 1.0f && pPlayer )
			{
				vecStart = pPlayer->EyePosition();

				ACTIVE_SPLITSCREEN_PLAYER_GUARD( pPlayer );
				QAngle	vangles;
				Vector	vforward, vright, vup;

				engine->GetViewAngles( vangles );
				AngleVectors( vangles, &vforward, &vright, &vup );

				VectorMA( vecStart, cl_righthand.GetBool() ? 2.5 : -2.5, vright, vecStart );
				VectorMA( vecStart, 10, vforward, vecStart );
				vecStart[2] -= 2.5f;
			}
		}

		// Create the particle effect
		QAngle vecAngles;
		Vector vecToEnd = vecEnd - vecStart;
		VectorNormalize(vecToEnd);
		VectorAngles( vecToEnd, vecAngles );
		DispatchParticleEffect( nParticleIndex, vecStart, vecEnd, vecAngles );
	}
	else
	{
		Vector vecStart = GetTracerOrigin( data );  
		vecEnd = data.m_vOrigin;
		QAngle dummy;
		
		if ( pViewModel )
		{
			C_BasePlayer *pPlayer = ToBasePlayer( ((C_BaseViewModel *)pViewModel)->GetOwner() );
			FormatViewModelAttachment( pPlayer, vecStart, true );
		}

		// if the tracer visually hits anything that it should not, just don't do the tracer
		trace_t tr;
		UTIL_TraceLine( vecStart, vecEnd, MASK_VISIBLE, pEntity, COLLISION_GROUP_DEBRIS, &tr );
		if ( tr.fraction == 1.0f )
			DispatchParticleEffect( nParticleIndex, vecStart, vecEnd, dummy );
		
	}

	if ( data.m_fFlags & TRACER_FLAG_WHIZ )
	{
		Vector vecStart = data.m_vStart;
		FX_TracerSound( vecStart, vecEnd, TRACER_TYPE_DEFAULT );	
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( ParticleTracer, ParticleTracerCallback );
	PRECACHE_INDEX( PARTICLE_SYSTEM, "weapon_tracers", s_nWeaponTracerIndex )
DECLARE_CLIENT_EFFECT_END()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TracerSoundCallback( const CEffectData &data )
{
	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	
	// Do tracer effect
	FX_TracerSound( vecStart, (Vector&)data.m_vOrigin, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( TracerSound, TracerSoundCallback );

