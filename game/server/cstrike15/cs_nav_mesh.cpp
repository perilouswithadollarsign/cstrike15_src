//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NavMesh.cpp
// Implementation of Navigation Mesh interface
// Author: Michael S. Booth, 2003-2004

#include "cbase.h"
#include "filesystem.h"
#include "cs_nav_mesh.h"
#include "cs_nav_node.h"
#include "cs_nav_area.h"
#include "cs_nav_pathfind.h"
#include "cs_shareddefs.h"
#include "cs_gamerules.h"
#include "fmtstr.h"
#include "utlbuffer.h"
#include "bot_util.h"
#include "tier0/vprof.h"
#include "cs_bot_manager.h"

#ifndef CLIENT_DLL
#include "mapinfo.h"
#endif

class CFuncBlockDMSpawns : public CFuncBrush
{
	DECLARE_CLASS( CFuncBlockDMSpawns, CFuncBrush );

	void Spawn( void )
	{
		SetSolid( SOLID_BSP );
		AddSolidFlags( FSOLID_NOT_SOLID );

		SetModel( STRING( GetModelName() ) );    // set size and link into world

		extern ConVar showtriggers;

		if ( !showtriggers.GetInt() )
		{
			AddEffects( EF_NODRAW );
		}
	}
};

LINK_ENTITY_TO_CLASS( func_block_dm_spawns, CFuncBlockDMSpawns );




extern ConVar mp_randomspawn;
ConVar mp_guardian_target_site( "mp_guardian_target_site", "-1", FCVAR_RELEASE | FCVAR_GAMEDLL, "If set to the index of a bombsite, will cause random spawns to be only created near that site." );

//--------------------------------------------------------------------------------------------------------------
CSNavMesh::CSNavMesh( void )
{
	m_desiredChickenCount = 0;
}

//--------------------------------------------------------------------------------------------------------------
CSNavMesh::~CSNavMesh()
{
}

CNavArea * CSNavMesh::CreateArea( void ) const
{
	return new CCSNavArea;
}

//-------------------------------------------------------------------------
void CSNavMesh::BeginCustomAnalysis( bool bIncremental )
{

}


//-------------------------------------------------------------------------
// invoked when custom analysis step is complete
void CSNavMesh::PostCustomAnalysis( void )
{

}


//-------------------------------------------------------------------------
void CSNavMesh::EndCustomAnalysis()
{

}


//-------------------------------------------------------------------------
/**
 * Returns sub-version number of data format used by derived classes
 */
unsigned int CSNavMesh::GetSubVersionNumber( void ) const
{
	// 1: initial implementation - added ApproachArea data
	return 1;
}

//-------------------------------------------------------------------------
/** 
 * Store custom mesh data for derived classes
 */
void CSNavMesh::SaveCustomData( CUtlBuffer &fileBuffer ) const
{

}

//-------------------------------------------------------------------------
/**
 * Load custom mesh data for derived classes
 */
void CSNavMesh::LoadCustomData( CUtlBuffer &fileBuffer, unsigned int subVersion )
{

}

//--------------------------------------------------------------------------------------------------------------
/**
 * Reset the Navigation Mesh to initial values
 */
void CSNavMesh::Reset( void )
{
	m_refreshChickenTimer.Start( 5.0f );
	m_refreshDMSpawnTimer.Start( 1.0f );

	CNavMesh::Reset();
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Zero player counts in all areas
 */
void CSNavMesh::ClearPlayerCounts( void )
{
	FOR_EACH_VEC( TheNavAreas, it )
	{
		CCSNavArea *area = (CCSNavArea*)TheNavAreas[ it ];
		area->ClearPlayerCount();
	}
}


//--------------------------------------------------------------------------------------------------------------
// Keep desired number of chickens alive in map
void CSNavMesh::MaintainChickenPopulation( void )
{
	if ( m_desiredChickenCount > 0 && m_refreshChickenTimer.IsElapsed() )
	{
		m_refreshChickenTimer.Start( RandomFloat( 10.0f, 20.0f ) );

		int actualCount = 0;
		for( int i=0; i<m_chickenVector.Count(); ++i )
		{
			if ( m_chickenVector[i] != NULL )
			{
				++actualCount;
			}
		}

		if ( actualCount < m_desiredChickenCount )
		{
			int need = m_desiredChickenCount - actualCount;

			for( int k=0; k<need; ++k )
			{
				// find a good spot to spawn a chicken
				CBaseEntity *chicken = NULL;

				for( int attempts=0; attempts<10; ++attempts )
				{
					int which = RandomInt( 0, TheNavAreas.Count()-1 );

					CNavArea *testArea = TheNavAreas[ which ];

					const float tooSmall = 50.0f;

					if ( testArea && testArea->GetSizeX() > tooSmall && testArea->GetSizeY() > tooSmall )
					{
						if ( !UTIL_IsVisibleToTeam( testArea->GetCenter(), TEAM_CT ) &&
							 !UTIL_IsVisibleToTeam( testArea->GetCenter(), TEAM_TERRORIST ) )
						{
							// don't spawn a chicken on top of another chicken
							int n;
							for( n=0; n<m_chickenVector.Count(); ++n )
							{
								if ( m_chickenVector[n] == NULL )
									continue;

								const float tooClose = 50.0f;
								Vector between = m_chickenVector[n]->GetAbsOrigin() - testArea->GetCenter();
								if ( between.IsLengthLessThan( tooClose ) )
									break;
							}

							if ( n >= m_chickenVector.Count() )
							{
								// found a good spot - spawn a chicken here
								chicken = CreateEntityByName( "chicken" );
								if ( chicken )
								{
									chicken->SetAbsOrigin( testArea->GetCenter() );

									DispatchSpawn( chicken );
									m_chickenVector.AddToTail( chicken );
								}

								break;
							}
						}
					}
				}

				if ( !chicken )
				{
					// couldn't spawn a chicken - try again later
					return;
				}
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void CSNavMesh::Update( void )
{
	MaintainChickenPopulation();

	MaintainDMSpawnPopulation();

	CNavMesh::Update();
}

NavErrorType CSNavMesh::Load( void )
{
	return CNavMesh::Load();
}

bool CSNavMesh::Save( void ) const
{
	return CNavMesh::Save();
}

NavErrorType CSNavMesh::PostLoad( unsigned int version )
{
	if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
		m_desiredChickenCount = 10;
	else
		m_desiredChickenCount = g_pMapInfo ? g_pMapInfo->m_iPetPopulation : 0;
	m_chickenVector.RemoveAll();

	if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() )
	{
		// in Guardian mode, remove all of the mapper placed DM spawn points
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "info_deathmatch_spawn" ) ) != NULL )
		{
			UTIL_Remove( pEntity );
		}
	}

	ResetDMSpawns();

	return CNavMesh::PostLoad(version);
}

CON_COMMAND( dm_reset_spawns, "" )
{
	( dynamic_cast< CSNavMesh* >( TheNavMesh ) )->ResetDMSpawns();
	DevMsg( "Manually resetting deathmatch spawns.\n");

}

void CSNavMesh::ResetDMSpawns( void )
{
	FOR_EACH_VEC( m_DMSpawnVector, i )
	{
		UTIL_Remove( m_DMSpawnVector[i] );
	}

	m_desiredDMSpawns = 50;
	m_consecutiveFailedAttempts = 0;
	m_DMSpawnVector.RemoveAll();
	MaintainDMSpawnPopulation();
}

void CSNavMesh::MaintainDMSpawnPopulation( void )
{
	if ( mp_randomspawn.GetInt() == 0 )
		return;

	bool bDisableAutoGeneratedDMSpawns = ( g_pMapInfo ? g_pMapInfo->m_bDisableAutoGeneratedDMSpawns : false );

	// HACK: Force this to run in guardian, even if the map explicitly placed dm spawns
	if ( bDisableAutoGeneratedDMSpawns && !CSGameRules()->IsPlayingCoopGuardian() )
		return;

	if ( m_desiredDMSpawns > 0 && m_refreshDMSpawnTimer.IsElapsed() )
	{
		m_refreshDMSpawnTimer.Start( 1.0f );

		int actualCount = 0;
		for( int i=0; i<m_DMSpawnVector.Count(); ++i )
		{
			if ( m_DMSpawnVector[i] != NULL )
			{
				++actualCount;
			}
		}

		// If the map isn't large enough to support as many spawns as m_desiredDMSpawns then shrink m_desiredDMSpawns to whatever we can get.
		if ( m_consecutiveFailedAttempts >= 100 )
		{
			m_desiredDMSpawns = actualCount;

			m_consecutiveFailedAttempts = 0;

			Warning( "Giving up attempts to make more random spawn points. Current count: %i.\n", actualCount );

		}

		if ( actualCount < m_desiredDMSpawns )
		{
//			float calctime = Plat_FloatTime(); MEASURE PERF 1/2

			int need = m_desiredDMSpawns - actualCount;

			int spawnsToTry = max( 10, need );

			for( int k=0; k < spawnsToTry; k++ )
			{
				// find a good spot to spawn a Deathmatch spawnpoint
				CBaseEntity *DMSpawn = NULL;

				m_consecutiveFailedAttempts++;

				for( int attempts = 0; attempts < 10; attempts++ )
				{

					CNavArea *testArea = NULL;
					if ( mp_guardian_target_site.GetInt() >= 0 )
					{
						const CCSBotManager::Zone *zone = TheCSBots()->GetZone( mp_guardian_target_site.GetInt() );
						if ( zone )
						{
							testArea = zone->m_area[ RandomInt( 0, zone->m_areaCount - 1 ) ];
						}
					}
					else
					{
						testArea = TheNavAreas[ RandomInt( 0, TheNavAreas.Count() - 1 ) ];
					}

					const float tooSmall = ( mp_guardian_target_site.GetInt() >= 0 ) ? 35.0f : 100.0f;

					if ( testArea && testArea->GetSizeX() > tooSmall && testArea->GetSizeY() > tooSmall )
					{
						bool bBadNavArea = false;

						// don't spawn a DMSpawn too close to another DMSpawn
						for( int n = 0; n < m_DMSpawnVector.Count(); n++ )
						{
							if ( m_DMSpawnVector[n] == NULL )
								continue;

							float tooClose = ( mp_guardian_target_site.GetInt() >= 0 ) ? 50.0f : 300.0f;
							Vector between = m_DMSpawnVector[n]->GetAbsOrigin() - testArea->GetCenter();
							if ( between.IsLengthLessThan( tooClose ) )
							{
								bBadNavArea = true;
								break;
							}
						}

						if ( bBadNavArea )
							continue;

						if ( IsSpawnBlockedByTrigger( testArea->GetCenter() + Vector( 0, 0, 16 ) ) )
							continue;

						// check that we can path from the nav area to a ct spawner to confirm it isn't orphaned.
						CBaseEntity *CTSpawn = gEntList.FindEntityByClassname( NULL, "info_player_counterterrorist" );

						if ( CTSpawn )
						{
							CNavArea *CTSpawnArea = GetNearestNavArea( CTSpawn->GetAbsOrigin() );

							ShortestPathCost cost;

							bool bNotOrphaned = NavAreaBuildPath( testArea, CTSpawnArea, NULL, cost );

							if ( !bNotOrphaned )
							{
								const char* szTSpawnEntName = "info_player_terrorist";
								if ( CSGameRules()->IsPlayingCoopMission() )
									szTSpawnEntName = "info_enemy_terrorist_spawn";

								// double check that we can path from the nav area to a t spawner to confirm it isn't orphaned.
								CBaseEntity *TSpawn = gEntList.FindEntityByClassname( NULL, szTSpawnEntName );
								if ( TSpawn )
								{
									CNavArea *TSpawnArea = GetNearestNavArea( TSpawn->GetAbsOrigin() );
									ShortestPathCost cost2;
									bNotOrphaned = NavAreaBuildPath( testArea, TSpawnArea, NULL, cost2 );

									if ( !bNotOrphaned )
										continue;
								}							
							}
						}

						// found a good spot - spawn a DMSpawn here
						DMSpawn = CreateEntityByName( "info_deathmatch_spawn" );
						if ( DMSpawn )
						{
							DMSpawn->SetAbsOrigin( testArea->GetCenter() + Vector( 0, 0, 16 ));

							DispatchSpawn( DMSpawn );
							m_DMSpawnVector.AddToTail( DMSpawn );

							m_consecutiveFailedAttempts = 0;

//							Msg( " %f :CREATED DM SPAWN %i.\n", gpGlobals->curtime, m_DMSpawnVector.Count() );
						}

						break;
					}
				}
			}

//			MEASURE PERF 2/2
// 			calctime = Plat_FloatTime() - calctime;
// 			Msg( "DM SPAWN SEARCH CALC TIME: %f\n", calctime );
		}
	}


}

bool CSNavMesh::IsSpawnBlockedByTrigger( Vector pos )
{
	CBaseEntity *list[32];

	Ray_t ray;
	ray.Init( pos, pos + Vector( 1,1,1 ) );

	int nCount = AllEdictsAlongRay( list, 1024, ray, 0 );

	for ( int i = 0; i < nCount; i++ )
	{
		if ( FClassnameIs( list[i], "func_block_dm_spawns" ) )
			return true;
	}

	return false;
}

int CSNavMesh::AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );

	::partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, &rayEnum );

	return rayEnum.GetCount();
}
