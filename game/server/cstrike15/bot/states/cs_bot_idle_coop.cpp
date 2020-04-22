//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Idle update function for coop mode. Split into different file for clarity
//
//===========================================================================//

#include "cbase.h"
#include "cs_bot.h"
#include "cs_team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar bot_coop_force_throw_grenade_chance( "bot_coop_force_throw_grenade_chance", "0.3", FCVAR_CHEAT );

bool ReactToBombState( CCSBot *me )
{
	const Vector* pVecPos = me->GetGameState()->GetBombPosition();
	if ( !pVecPos )
		return false;

	if ( me->GetTeamNumber() == TEAM_TERRORIST )
	{
		if ( TheCSBots()->IsBombPlanted() )
			me->SetTask( CCSBot::GUARD_TICKING_BOMB );
		else if ( me->GetGameState()->IsBombLoose() )
		{
			me->FetchBomb();
			return true;
		}
		else
			return false;
	}
	else if ( me->GetTeamNumber() == TEAM_CT )
	{
		if ( TheCSBots()->IsBombPlanted() )
			me->SetTask( CCSBot::DEFUSE_BOMB );
		else if ( me->GetGameState()->IsBombLoose() )
			me->SetTask( CCSBot::GUARD_LOOSE_BOMB );
		/*
		else if ( me->GetGameState()->GetBombState() == CSGameState::MOVING )
		{
			float flDistToSite0 = ( *pVecPos - TheCSBots()->GetZone( 0 )->m_center ).Length2D();
			float flDistToSite1 = ( *pVecPos - TheCSBots()->GetZone( 1 )->m_center ).Length2D();
		} */
		else
			return false;
	}

	me->MoveTo( *pVecPos );
	me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

	return true;
}

// Terrorists rush the chosen site
void RunStrat_Rush( CCSBot * me, int iBombSite )
{
	if ( !ReactToBombState( me ) )
	{
		const CCSBotManager::Zone *zone = TheCSBots()->GetZone( iBombSite );
		const Vector *pos = TheCSBots()->GetRandomPositionInZone( zone );
		if ( !pos)
		{
			Warning( "ERROR: Map either has < 2 bomb sites, or one of the sites has no areas. Coop bots will be broken." );
			return;
		}

		if ( me->IsAtBombsite() )
		{
			bool bIsSafe = gpGlobals->curtime - me->GetLastSawEnemyTimestamp() > 2.0f; // TODO: Might be better to use enemy death/remaining count for this
			if ( me->HasC4() && bIsSafe )
			{
				me->SetTask( CCSBot::PLANT_BOMB );
			}
			else
			{
				Place place = TheNavMesh->GetPlace( zone->m_center );
				if ( const Vector* pVecHidingSpot = FindRandomHidingSpot( me, place, me->IsSniper() ) )
				{
					pos = pVecHidingSpot;
				}
				me->SetTask( CCSBot::GUARD_BOMB_ZONE );
			}
		}
		else
		{
			me->SetTask( CCSBot::SEEK_AND_DESTROY );
		}

		me->MoveTo( *pos );
		me->Run();
		me->PrintIfWatched( "I'm rushing the bomb site\n" );
		me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
	}
}

void RunStrat_GuardSpot( CCSBot *me )
{
	if ( !ReactToBombState( me ) )
	{
		const CCSBotManager::Zone *zone = TheCSBots()->GetRandomZone();
		if ( zone )
		{
			CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
			if ( area )
			{
				me->PrintIfWatched( "I'm guarding a bombsite\n" );
				me->GetChatter()->GuardingBombsite( area->GetPlace() );
				me->SetTask( CCSBot::GUARD_BOMB_ZONE );
				me->Hide( area, -1.0, 1000.0f );
				me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
			}
		}
	}
}


void MakeAwareOfCTs( CCSBot * me )
{
	CUtlVector< CCSPlayer * > vecCTs;
	CollectPlayers( &vecCTs, TEAM_CT, COLLECT_ONLY_LIVING_PLAYERS );

	if ( vecCTs.Count() == 0 )
		return;

	CCSPlayer *pTargetPlayer = NULL;
	float flClosePlayer = 1e6;
	FOR_EACH_VEC( vecCTs, iter )
	{
		if ( me->IsVisible( vecCTs[ iter ] ) )
		{
			float dist = me->GetTravelDistanceToPlayer( vecCTs[ iter ] );
			if ( dist < flClosePlayer )
			{
				flClosePlayer = dist;
				pTargetPlayer = vecCTs[ iter ];
			}
		}

		me->OnAudibleEvent( NULL, vecCTs[ iter ], MAX_COORD_FLOAT, PRIORITY_HIGH, true );
	}

	if ( !pTargetPlayer )
		pTargetPlayer = vecCTs[ RandomInt( 0, vecCTs.Count() - 1 ) ];

	me->SetBotEnemy( pTargetPlayer );
}


extern ConVar mp_guardian_target_site;
void IdleState::UpdateCoop( CCSBot *me )
{
	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		Assert( mp_guardian_target_site.GetInt() >= 0 );
		// if this is a bomb game and we have the bomb, go plant it
		if ( me->GetTeamNumber() == TEAM_TERRORIST )
		{
			switch ( TheCSBots()->GetTStrat() )
			{
				case CCSBotManager::k_ETStrat_Rush:
				{
					RunStrat_Rush( me, TheCSBots()->GetTerroristTargetSite() );
					break;
				}
			}
		}
		else if ( me->GetTeamNumber() == TEAM_CT )
		{
			switch ( TheCSBots()->GetCTStrat() )
			{
				case CCSBotManager::k_ECTStrat_StackSite:
				{
					RunStrat_GuardSpot( me );
					break;
				}
			}
		}
	}
	else if ( CSGameRules()->IsPlayingCoopMission() )
	{
		if ( me->GetTeamNumber() == TEAM_TERRORIST )
		{
			SpawnPointCoopEnemy* pSpawn = me->GetLastCoopSpawnPoint();
			if ( !pSpawn )
				return;

			// if we're holding a grenade, throw it at the victim
			if ( me->HasGrenade() && CSGameRules()->IsPlayingCooperativeGametype() && RandomFloat() < bot_coop_force_throw_grenade_chance.GetFloat() )
				me->EquipGrenade();

			// HACK: We'd like last guy alive to stop hiding and hunt down the players
			// but don't want the first spawned guy to go straight to charging... kick this clause into the future a few seconds
			// to make sure our wave is in before making this check. 
			SpawnPointCoopEnemy::BotDefaultBehavior_t behavior = pSpawn->GetDefaultBehavior();
			if ( me->GetFriendsRemaining() == 0 && gpGlobals->curtime - me->m_spawnedTime > 5.0f )
				behavior = SpawnPointCoopEnemy::CHARGE_ENEMY;

			switch ( behavior )
			{
				case SpawnPointCoopEnemy::HUNT:
				{
					me->Hunt();
					break;
				}
				case SpawnPointCoopEnemy::DEFEND_AREA:
				case SpawnPointCoopEnemy::DEFEND_INVESTIGATE:
				{
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					if ( me->IsSniper() )
						me->SetTask( CCSBot::MOVE_TO_SNIPER_SPOT );
					else
						me->SetTask( CCSBot::HOLD_POSITION );

					if ( pSpawn->HideRadius() > 0 )
					{
						if ( !me->TryToHide( pSpawn->FindNearestArea(), -1.0, pSpawn->HideRadius(), true, false, &me->GetAbsOrigin() ) )
						{
							// This spawn isn't hideable, stop trying
							pSpawn->HideRadius( 0 );
						}
					}
					else
					{
						// look around
						me->UpdateLookAround();

						if ( pSpawn->GetDefaultBehavior() == SpawnPointCoopEnemy::DEFEND_INVESTIGATE )
						{
							me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
							// listen for enemy noises
							if ( me->HeardInterestingNoise() )
							{
								me->InvestigateNoise();
								pSpawn->SetDefaultBehavior( SpawnPointCoopEnemy::HUNT );
							}
						}
					}
					break;
				}
				case SpawnPointCoopEnemy::CHARGE_ENEMY:
				{
					MakeAwareOfCTs( me );

					me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
					
					if ( me->GetBotEnemy() )
					{
						me->SetTask( CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION, me->GetBotEnemy() );
						me->MoveTo( me->GetBotEnemy()->GetAbsOrigin() );
					}
					
					break;
				}
			}
		}
	}
}