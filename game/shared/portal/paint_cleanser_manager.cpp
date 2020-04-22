//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#include "cbase.h"

#include "paint_cleanser_manager.h"
#include "igameevents.h"

#ifdef CLIENT_DLL
	#include "c_trigger_paint_cleanser.h"
	#include "debugoverlay_shared.h"

	ConVar paint_cleanser_visibility_poll_frequency( "paint_cleanser_visibility_poll_rate", "0.5", FCVAR_CHEAT );
	ConVar paint_cleanser_visibility_checks_debug( "paint_cleanser_visibility_checks_debug", "0", FCVAR_DEVELOPMENTONLY );
	ConVar paint_cleanser_visibility_range( "paint_cleanser_visibility_range", "1000.0f", FCVAR_CHEAT );
	ConVar paint_cleanser_visibility_look_angle( "paint_cleanser_visibility_look_angle", "60.0f", FCVAR_CHEAT );
#else
	#include "trigger_paint_cleanser.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CPaintCleanserManager PaintCleanserManager( "PaintCleanserManager" );

CPaintCleanserManager::CPaintCleanserManager( char const *name )
#ifdef CLIENT_DLL
					: CAutoGameSystemPerFrame( name ),
					  m_flNextPollTime( 0.0f )
#else
					: CAutoGameSystem( name )
#endif
{
#ifdef CLIENT_DLL
	memset( m_ppVisibleCleanser, 0, sizeof(m_ppVisibleCleanser) );
#endif
}


CPaintCleanserManager::~CPaintCleanserManager( void )
{
}


void CPaintCleanserManager::LevelInitPreEntity()
{
#ifdef CLIENT_DLL
	m_flNextPollTime = gpGlobals->curtime + paint_cleanser_visibility_poll_frequency.GetFloat();
#endif
}


void CPaintCleanserManager::LevelShutdownPreEntity()
{
	m_PaintCleansers.RemoveAll();
}


void CPaintCleanserManager::AddPaintCleanser( C_TriggerPaintCleanser *pCleanser )
{
	m_PaintCleansers.AddToTail( pCleanser );
}


void CPaintCleanserManager::RemovePaintCleanser( C_TriggerPaintCleanser *pCleanser )
{
	m_PaintCleansers.FindAndRemove( pCleanser );
}


void CPaintCleanserManager::GetPaintCleansers( PaintCleanserVector_t& paintCleansers )
{
	paintCleansers = m_PaintCleansers;
}


#ifdef CLIENT_DLL
void CPaintCleanserManager::Update( float frametime )
{
	//Check if it is time to update the cleanser visibility
	if( gpGlobals->curtime < m_flNextPollTime )
	{
		return;
	}

	//Set the time for the next check
	m_flNextPollTime = gpGlobals->curtime + paint_cleanser_visibility_poll_frequency.GetFloat();

	UpdatePaintCleanserVisibility();
}


void CPaintCleanserManager::UpdatePaintCleanserVisibility( void )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer( hh );
		if( !pPlayer )
			continue;

		Vector vecPlayerForward;
		AngleVectors( pPlayer->EyeAngles(), &vecPlayerForward );
		VectorNormalize( vecPlayerForward );
		Vector vecPlayerEyePos = pPlayer->EyePosition();

		//Max distance to check for visibility
		float flVisibilityRange = paint_cleanser_visibility_range.GetFloat() * paint_cleanser_visibility_range.GetFloat();

		bool bDebugging = paint_cleanser_visibility_checks_debug.GetBool();

		float flClosestCleanserDist = FLT_MAX;
		C_TriggerPaintCleanser *pClosestCleanser = NULL;

		//If there is a cleanser visible before the check
		bool bCleanserWasVisible = m_ppVisibleCleanser[hh] ? true : false;

		for( int i = 0; i < m_PaintCleansers.Count(); ++i )
		{
			Vector vecCleanserPos = m_PaintCleansers[i]->WorldSpaceCenter();

			//Don't test the cleanser if the player is too far from it
			float flDistSqr = vecPlayerEyePos.DistToSqr( vecCleanserPos );
			if( flDistSqr > flVisibilityRange )
			{
				if( m_ppVisibleCleanser[hh] == m_PaintCleansers[i] )
				{
					m_ppVisibleCleanser[hh] = NULL;
				}

				continue;
			}

			//Don't test the cleanser if the player isn't looking in its direction
			Vector vecPlayerCleanserDir = vecCleanserPos - vecPlayerEyePos;
			VectorNormalize( vecPlayerCleanserDir );
			float flPlayerCleanserAngle = RAD2DEG( acos( DotProduct( vecPlayerForward, vecPlayerCleanserDir ) ) );
			if( flPlayerCleanserAngle >= paint_cleanser_visibility_look_angle.GetFloat() )
			{
				if( m_ppVisibleCleanser[hh] == m_PaintCleansers[i] )
				{
					m_ppVisibleCleanser[hh] = NULL;
				}

				continue;
			}

			//Trace from the player's eye to the paint cleanser
			Ray_t playerCleanserRay;
			playerCleanserRay.Init( vecPlayerEyePos, vecCleanserPos );
			trace_t tr;
			int mask = MASK_BLOCKLOS_AND_NPCS & ~CONTENTS_BLOCKLOS;
			UTIL_TraceRay( playerCleanserRay, mask, pPlayer, COLLISION_GROUP_NONE, &tr );

			//If there is nothing between the player and the cleanser
			if( tr.fraction == 1.0f || tr.m_pEnt == m_PaintCleansers[i] )
			{
				if( bDebugging )
				{
					NDebugOverlay::Line( pPlayer->WorldSpaceCenter(), vecCleanserPos, 0, 255, 255, false, paint_cleanser_visibility_poll_frequency.GetFloat() );
				}

				Color debugColor( 255, 0, 0 );

				//If this is the closest visible paint cleanser
				if( flDistSqr < flClosestCleanserDist )
				{
					flClosestCleanserDist = flDistSqr;
					pClosestCleanser = m_PaintCleansers[i];

					debugColor = Color( 0, 255, 0 );
				}

				if( bDebugging )
				{
					NDebugOverlay::Cross3D( vecCleanserPos, 16, debugColor.r(), debugColor.g(), debugColor.b(), false, paint_cleanser_visibility_poll_frequency.GetFloat() );
				}
			}
			else
			{
				if( bDebugging )
				{
					NDebugOverlay::Line( pPlayer->WorldSpaceCenter(), vecCleanserPos, 0, 0, 255, false, paint_cleanser_visibility_poll_frequency.GetFloat() );
				}
			}
		} //For all the paint cleansers

		//If the player can see a different cleanser
		if( pClosestCleanser && pClosestCleanser != m_ppVisibleCleanser[hh] )
		{
			m_ppVisibleCleanser[hh] = pClosestCleanser;

			if( pPlayer->Weapon_OwnsThisType( "weapon_paintgun" ) )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "paint_cleanser_visible" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					event->SetInt( "subject", m_ppVisibleCleanser[hh]->entindex() );

					gameeventmanager->FireEventClientSide( event );
				}
			}
		}
		//If there is no visible cleanser
		else if( !m_ppVisibleCleanser[hh] && bCleanserWasVisible )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "paint_cleanser_not_visible" );
			if( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );

				gameeventmanager->FireEventClientSide( event );
			}
		}

	} //for each pPlayer
}
#endif
