//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A game system for tracking and updating entity spot state
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "cs_entity_spotting.h"
#include "cs_player.h"
#include "cs_bot.h"
#include "sensorgrenade_projectile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ENTITY_SPOT_FREQUENCY 0.5f

static CCSEntitySpotting s_EntitySpotting("CCSEntitySpotting");
CCSEntitySpotting * g_EntitySpotting = &s_EntitySpotting;

ConVar radarvis( "radarvismethod", "1", FCVAR_CHEAT, "0 for traditional method, 1 for more realistic method", true, 0, true, 1 );
ConVar radarpow( "radarvispow", ".4", FCVAR_CHEAT, "the degree to which you can point away from a target, and still see them on radar." );
ConVar radardist( "radarvisdistance", "1000.0f", FCVAR_CHEAT, "at this distance and beyond you need to be point right at someone to see them", true, 10, false, 0 );
ConVar radarmaxdot( "radarvismaxdot", ".996", FCVAR_CHEAT, "how closely you have to point at someone to see them beyond max distance", true, 0, true, 1.0f );


//--------------------------------------------------------------------------------------------------------
// Functors
//--------------------------------------------------------------------------------------------------------

template < typename SpotFunctor >
bool ForEachEntitySpotter( SpotFunctor &func )
{
	VPROF( "ForEachEntitySpotter" );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast< CBasePlayer * >( UTIL_PlayerByIndex( i ) );

		if ( player == NULL )
			continue;

		if ( FNullEnt( player->edict() ) )
			continue;

		if ( !player->IsPlayer() )
			continue;

		if ( !player->IsConnected() )
			continue;

		if ( func( player ) == false )
			return false;
	}

	//ActiveGrenadeList activeGrenadeList = TheBots->m_activeGrenadeList;
	FOR_EACH_LL( TheCSBots()->m_activeGrenadeList, it )
	{
		ActiveGrenade *ag = TheCSBots()->m_activeGrenadeList[it];
		if ( ag->IsSensor() == false )
			continue;

		CBaseGrenade *grenade = ag->GetEntity();
		if ( grenade == NULL )
			continue;
		
		if ( FNullEnt( grenade->edict() ) )
			continue;

		if ( func( grenade ) == false )
			return false;
	}

	/*
	CBaseEntity * pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		if ( pEntity == NULL || FNullEnt( pEntity->edict() ) )
		{
			pEntity = gEntList.NextEnt( pEntity );
			continue;
		}

		if ( FNullEnt( pEntity->edict() ) )
		{
			pEntity = gEntList.NextEnt( pEntity );
			continue;
		}

		if ( pEntity->IsPlayer() )
		{
			CBasePlayer *player = static_cast< CBasePlayer * >( pEntity );

			if ( !player->IsConnected() )
			{
				pEntity = gEntList.NextEnt( pEntity );
				continue;
			}
		}

		if ( func( pEntity ) == false )
			return false;

		// 		else if ( dynamic_cast<CSensorGrenadeProjectile*>( pEntity ) )
		// 		{
		// 			if ( pThrower->IsOtherEnemy( pPlayer ) )
		// 			{
		// 				Vector vDelta = pPlayer->EyePosition() - GetAbsOrigin();
		// 				float flDistance = vDelta.Length();
		// 
		// 				float flMaxDrawDist = 1024;
		// 				if ( flDistance <= flMaxDrawDist )
		// 				{
		// 					trace_t tr;
		// 					//if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
		// 					{
		// 						if ( TheCSBots()->IsLineBlockedBySmoke( pPlayer->EyePosition(), GetAbsOrigin(), 1.0f ) )
		// 						{
		// 							continue;
		// 						}
		// 
		// 						UTIL_TraceLine( pPlayer->EyePosition(), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr );
		// 						if ( tr.fraction != 1 )
		// 						{
		// 							trace_t tr2;
		// 							UTIL_TraceLine( pPlayer->GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin(), MASK_VISIBLE, pPlayer, COLLISION_GROUP_DEBRIS, &tr2 );
		// 							if ( tr2.fraction != 1 )
		// 							{
		// 								continue;
		// 							}
		// 						}
		// 
		// 						pPlayer->SetIsSpotted( true );
		// 						pPlayer->SetIsSpottedBy( nThrowerIndex );
		// 					}
		// 				}
		// 			}
		// 		}

		pEntity = gEntList.NextEnt( pEntity );
	}*/

	return true;
}
//==================================================
// - CanPlayerSeeTargetEntityFunctor -
//
// Determine if a player has spotted a target entity.
// Query IsSpotted() for result
//==================================================

class CanPlayerSeeTargetEntityFunctor
{
public:
	CanPlayerSeeTargetEntityFunctor( CBaseEntity *entity, int spottingTeam )
	{
		m_targetEntity = entity;
		m_target = entity->EyePosition();
		m_team = spottingTeam;
		m_spotted = false;
	}

	bool operator()( CBaseEntity *spotter )
	{
		CCSPlayer *csPlayer = ToCSPlayer( spotter );
		if ( (csPlayer && csPlayer->IsAlive() == false ) || spotter->GetTeamNumber() != m_team )
			return true;

		CCSPlayer *csPlayerSpotter = NULL;

		bool doTrace = false;
		Vector eye, forward;
		if ( csPlayer )
		{
			csPlayerSpotter = csPlayer;

			if ( csPlayer->IsBlind() )
				return true;
	
			csPlayer->EyePositionAndVectors( &eye, &forward, NULL, NULL );
			Vector path( m_target - eye );
			float distance = path.Length();
			path.NormalizeInPlace();
			float dot = DotProduct( forward, path );

			if ( dot < 0 )
				return true;

			int rvm = radarvis.GetInt();

			switch ( rvm )
			{
				case 0:// original method
					doTrace = ( ( dot > 0.995f )
								|| ( dot > 0.98f && distance < 900 )
								|| ( dot > 0.8f && distance < 250 )
								);
					break;

				case 1: // new method method
				{
							int fov = csPlayer->GetFOVForNetworking() / 2;
							float cosfov = cosf( ( float )fov*3.1415f / 180.0f );

							float d = distance / radardist.GetFloat();
							d = clamp( powf( d, radarpow.GetFloat() ), cosfov, radarmaxdot.GetFloat() );

							doTrace = ( dot > d );
				}
					break;

			}
		}
		else if ( dynamic_cast<CSensorGrenadeProjectile*>( spotter ) )
		{
			//CCSPlayer *csTargetPlayer = ToCSPlayer( m_targetEntity );
			//if ( spotter->IsOtherEnemy( m_targetEntity ) )
			{
				eye = spotter->GetAbsOrigin();
				csPlayerSpotter = ToCSPlayer( dynamic_cast<CSensorGrenadeProjectile*>( spotter )->GetThrower() );
				doTrace = true;

				/*
				Vector vDelta = csTargetPlayer->EyePosition() - spotter->GetAbsOrigin();
				float flDistance = vDelta.Length();

				float flMaxDrawDist = 1024;
				if ( flDistance <= flMaxDrawDist )
				{
					trace_t tr;
					//if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
					{
						if ( TheCSBots()->IsLineBlockedBySmoke( csTargetPlayer->EyePosition(), GetAbsOrigin(), 1.0f ) == false )
						{
							UTIL_TraceLine( csTargetPlayer->EyePosition(), GetAbsOrigin(), MASK_VISIBLE, csTargetPlayer, COLLISION_GROUP_DEBRIS, &tr );
							trace_t tr2;
							UTIL_TraceLine( csTargetPlayer->GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsOrigin(), MASK_VISIBLE, csTargetPlayer, COLLISION_GROUP_DEBRIS, &tr2 );
							if ( tr1.fraction == 1 || tr2.fraction == 1 )
								{
									continue;
								}
							}
						}

						pPlayer->SetIsSpotted( true );
						pPlayer->SetIsSpottedBy( nThrowerIndex );
					}
				}
				*/
			}
		}

		if (doTrace && csPlayerSpotter)
		{
			trace_t tr;
			CTraceFilterSkipTwoEntities filter( spotter, m_targetEntity, COLLISION_GROUP_DEBRIS );
			UTIL_TraceLine( eye, m_target,
				(CONTENTS_OPAQUE|CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_DEBRIS|MASK_OPAQUE_AND_NPCS), &filter, &tr );

			if ( tr.fraction == 1.0f && TheCSBots()->IsLineBlockedBySmoke( eye, m_target, 1.0f ) == false )
			{
				if ( !csPlayer || (csPlayer && csPlayer->GetFogObscuredRatio( m_targetEntity ) < 0.9) )
				{
					m_spotted = true;
					m_spottedBy.AddToTail( csPlayerSpotter->entindex() );
					//return false; // spotted already, so no reason to check for other players spotting the same thing.
				}
			}
		}

		return true;
	}

	bool IsSpotted( void ) const
	{
		return m_spotted;
	}

	int GetIsSpottedBy( int nUtlIndex ) const
	{
		if ( nUtlIndex < m_spottedBy.Count() )
			return m_spottedBy[nUtlIndex];

		return 0;
	}

	int GetSpottedByCount( void ) const
	{
		return m_spottedBy.Count();
	}

private:
	CBaseEntity *m_targetEntity;
	Vector m_target;
	int m_team;
	bool m_spotted;
	CUtlVector < int > m_spottedBy;
};


#define BIT_SET( a, b ) ((a)[(b)>>3] & (1<<((b)&7)))

extern bool WasPlayerOccluded( int fromplayer, int toplayer );


//===========================================================
// - GatherNonPVSSpottedEntitiesFunctor -
//
// Given a player, generate a list of spotted entities that
// exist outside of that player's PVS.
// Query GetSpotted for result
//===========================================================

GatherNonPVSSpottedEntitiesFunctor::GatherNonPVSSpottedEntitiesFunctor( CCSPlayer * pPlayer ) : m_pPlayer( pPlayer )
{
	if ( pPlayer )
	{
		m_nSourceTeam = pPlayer->GetAssociatedTeamNumber();

		engine->GetPVSForCluster( engine->GetClusterForOrigin( pPlayer->EyePosition() ), sizeof( m_pSourcePVS ), m_pSourcePVS );

		// spectators and OBS_ALLOW_ALL observers receive updates on all spottable entities 
		if ( m_nSourceTeam == TEAM_SPECTATOR )
		{
			m_bForceSpot = true;
		}
		else if ( pPlayer->GetObserverMode() != OBS_MODE_NONE )
		{
			ConVarRef mp_forcecamera( "mp_forcecamera" );
			m_bForceSpot = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;
		}
		else
		{
			m_bForceSpot = false;
		}
	}
}

bool GatherNonPVSSpottedEntitiesFunctor::operator()( CBaseEntity * pEntity )
{
	if ( !pEntity->edict() || !pEntity->CanBeSpotted() )
	{
		return true;
	}

	CBaseEntity *pParent = pEntity->GetRootMoveParent();

	int iBitNumber = engine->GetClusterForOrigin( pParent->EyePosition() );

	// We only care about entities who are not within this player's PVS
	// We include being occluded as being outside of PVS.
	if ( !BIT_SET( m_pSourcePVS, iBitNumber ) || ( m_pPlayer && pParent->entindex() <= MAX_PLAYERS && WasPlayerOccluded( pParent->entindex(), m_pPlayer->entindex() ) ) )
	{
		// target outside of PVS
		int nSpotRules = pEntity->GetSpotRules();
		bool bForceSpotted = false;

		if ( ( nSpotRules & CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_FRIEND ) &&
			 ( pEntity->GetTeamNumber() == m_nSourceTeam ) )
			bForceSpotted = true;

		if ( ( nSpotRules & CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_CT ) &&
			( TEAM_CT == m_nSourceTeam ) )
			bForceSpotted = true;

		if ( ( nSpotRules & CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T ) &&
			( TEAM_TERRORIST == m_nSourceTeam  ) )
			bForceSpotted = true;

		CBasePlayer * pPlayer = UTIL_PlayerByIndex( pEntity->entindex() );
		if ( pPlayer )
		{
			// do not include dead players, observers, etc
			if ( !pPlayer->IsAlive() || pPlayer->IsObserver() || !pPlayer->IsConnected() )
			{
				return true;
			}
		}

		m_EntitySpotted.Set( pEntity->entindex(), ( m_bForceSpot || pEntity->IsSpotted() || bForceSpotted ) );
	}

	return true;
}







//--------------------------------------------------------------------------------------------------------
// Entity Spotting Game System
//--------------------------------------------------------------------------------------------------------


CCSEntitySpotting::CCSEntitySpotting( const char * szName ) : CAutoGameSystemPerFrame( szName ),
	m_fLastUpdate( 0.0f )
{

}


void CCSEntitySpotting::FrameUpdatePostEntityThink( void )
{
	if ( gpGlobals->curtime > ( m_fLastUpdate + ENTITY_SPOT_FREQUENCY ) )
	{
		UpdateSpottedEntities();
	}
}


void CCSEntitySpotting::UpdateSpottedEntities( void )
{
	m_fLastUpdate = gpGlobals->curtime;

	CBaseEntity * pEntity = gEntList.FirstEnt();
	while ( pEntity )
	{
		if ( pEntity->CanBeSpotted() )
		{
			int nTeamID = 0;

			if ( pEntity->GetSpotRules() & SPOT_RULE_ENEMY )
			{
				nTeamID = (pEntity->GetTeamNumber( ) == TEAM_CT) ? TEAM_TERRORIST : TEAM_CT;
			}
			else if ( pEntity->GetSpotRules() & SPOT_RULE_CT )
			{
				nTeamID = TEAM_CT;
			}
			else if ( pEntity->GetSpotRules() & SPOT_RULE_T )
			{
				nTeamID = TEAM_TERRORIST;
			}

			CanPlayerSeeTargetEntityFunctor canPlayerSeeTargetEntity( pEntity, nTeamID  );
			ForEachEntitySpotter( canPlayerSeeTargetEntity );
			
			pEntity->SetIsSpotted( canPlayerSeeTargetEntity.IsSpotted( ) );
			pEntity->ClearSpottedBy();
			if ( canPlayerSeeTargetEntity.IsSpotted() )
			{
				for ( int i = 0; i < canPlayerSeeTargetEntity.GetSpottedByCount(); i++ )
				{
					pEntity->SetIsSpottedBy( canPlayerSeeTargetEntity.GetIsSpottedBy( i ) );
				}
			}
		}

		pEntity = gEntList.NextEnt( pEntity );
	}
}


bool CCSEntitySpotting::Init( void )
{
	return true;
}





