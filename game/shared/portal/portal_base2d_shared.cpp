//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "portal_base2d_shared.h"
#include "portal_shareddefs.h"
#include "mathlib/polyhedron.h"
#include "tier1/callqueue.h"
#include "debugoverlay_shared.h"
#include "collisionutils.h"

#if defined( CLIENT_DLL )
#include "c_portal_player.h"
#else
#include "portal_player.h"
#include "portal_physics_collisionevent.h"
#endif

#ifdef CLIENT_DLL
#include "c_basedoor.h"
#include "prediction.h"
#else
#include "env_debughistory.h"
#include "portal/weapon_physcannon.h"
#include "physics_bone_follower.h"
#include "projectedwallentity.h"
#include "portal_physics_collisionevent.h"
#endif

CUtlVector<CPortal_Base2D *> CPortal_Base2D_Shared::AllPortals;

void MobilePortalsUpdatedCallback( IConVar *var, const char *pOldValue, float flOldValue );
ConVar sv_allow_mobile_portals( "sv_allow_mobile_portals", "0", FCVAR_REPLICATED, "", MobilePortalsUpdatedCallback );

void MobilePortalsUpdatedCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( sv_allow_mobile_portals.GetFloat() == flOldValue )
		return;

	static ConVarRef sv_cheats ( "sv_cheats" );
	bool bCheatsAllowed = (sv_cheats.IsValid() && sv_cheats.GetBool() );

	if ( !bCheatsAllowed )
	{
#ifdef CLIENT_DLL
		if ( V_stricmp( engine->GetLevelNameShort(), "sp_a2_bts5" ) == 0 )
#else
		if ( V_stricmp( gpGlobals->mapname.ToCStr(), "sp_a2_bts5" ) == 0 )
#endif
		{
			bCheatsAllowed = true;
		}
	}

	if ( !bCheatsAllowed )
	{
		var->SetValue( 0 );
	}
}

ConVar sv_allow_mobile_portal_teleportation( "sv_allow_mobile_portal_teleportation", "1", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar sv_portal_unified_velocity( "sv_portal_unified_velocity", "1", FCVAR_CHEAT | FCVAR_REPLICATED, "An attempt at removing patchwork velocity tranformation in portals, moving to a unified approach." );
ConVar sv_bowie_maneuver_threshold( "sv_bowie_maneuver_threshold", "375.0f", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar sv_futbol_floor_exit_angle( "sv_futbol_floor_exit_angle", "85", FCVAR_CHEAT | FCVAR_REPLICATED );
extern ConVar sv_portal_debug_touch;

extern CCallQueue *GetPortalCallQueue();

#if defined( GAME_DLL )
extern CPortal_CollisionEvent g_Collisions;
ConVar sv_portal_teleportation_resets_collision_events( "sv_portal_teleportation_resets_collision_events", "1", FCVAR_CHEAT );
#endif

void CPortal_Base2D_Shared::UpdatePortalTransformationMatrix( const matrix3x4_t &localToWorld, const matrix3x4_t &remoteToWorld, VMatrix *pMatrix )
{
	VMatrix matPortal1ToWorldInv, matRotation;

	//inverse of this
	MatrixInverseTR( VMatrix( localToWorld ), matPortal1ToWorldInv );

	//180 degree rotation about up
	matRotation.Identity();
	matRotation.m[0][0] = -1.0f;
	matRotation.m[1][1] = -1.0f;

	VMatrix vTest = matRotation * matPortal1ToWorldInv;

	//final
	VMatrix matPortal2ToWorld( remoteToWorld );	
	*pMatrix = matPortal2ToWorld * matRotation * matPortal1ToWorldInv;
}

static char *g_pszPortalNonTeleportable[] = 
{ 
	"func_door", 
	"func_door_rotating", 
	"prop_door_rotating",
	"func_movelinear",
	"func_tracktrain",
	//"env_ghostanimating",
	"physicsshadowclone",
	"prop_ragdoll",
	"physics_prop_ragdoll",
	"func_brush"
};

bool CPortal_Base2D_Shared::IsEntityTeleportable( CBaseEntity *pEntity )
{
	switch( pEntity->GetMoveType() )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
		return false;
	};

	if( pEntity->GetMoveParent() != NULL )
		return false;

#ifdef CLIENT_DLL
		//client
	
		if( dynamic_cast<C_BaseDoor *>(pEntity) != NULL )
			return false;

#else
		//server
		if( dynamic_cast<CBoneFollower *>(pEntity) != NULL )
			return false;

		CPortal_Player *pHoldingPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pEntity );
		if ( pHoldingPlayer && pHoldingPlayer->IsUsingVMGrab() )
		{
			// No need to teleport with viewmodel grab controller
			return false;
		}
		
		for( int i = 0; i != ARRAYSIZE(g_pszPortalNonTeleportable); ++i )
		{
			if( FClassnameIs( pEntity, g_pszPortalNonTeleportable[i] ) )
				return false;
		}

#endif

	return true;
}


static char *g_pszPortalPhysicsCloneTouching[] = 
{ 
	"func_brush",
	"projected_wall_entity"
};

bool CPortal_Base2D_Shared::ShouldPhysicsCloneNonTeleportableEntityAcrossPortals( CBaseEntity *pEntity )
{
	for( int i = 0; i != ARRAYSIZE(g_pszPortalPhysicsCloneTouching); ++i )
	{
		if( FClassnameIs( pEntity, g_pszPortalPhysicsCloneTouching[i] ) )
		{
			return true;
		}
	}

	return false;
}


//unify how we determine the velocity of objects when portalling them
Vector Portal_FindUsefulVelocity( CBaseEntity *pOther )
{
	Vector vOtherVelocity;
	IPhysicsObject *pOtherPhysObject = pOther->VPhysicsGetObject();
	if( sv_portal_unified_velocity.GetBool() )
	{
		if( (pOther->GetMoveType() == MOVETYPE_VPHYSICS) && (pOtherPhysObject != NULL) )
		{
			pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );
		}
		else
		{
			vOtherVelocity = pOther->GetAbsVelocity();
			if( pOtherPhysObject )
			{
				Vector vPhysVelocity;
				pOtherPhysObject->GetVelocity( &vPhysVelocity, NULL );

				if( vPhysVelocity.LengthSqr() > vOtherVelocity.LengthSqr() )
				{
					vOtherVelocity = vPhysVelocity;
				}
			}
		}
	}
	else
	{
		if( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
		{
			if( pOtherPhysObject && (pOtherPhysObject->GetShadowController() == NULL) )
			{
				pOtherPhysObject->GetVelocity( &vOtherVelocity, NULL );
			}
			else
			{
#if defined( GAME_DLL )
				pOther->GetVelocity( &vOtherVelocity );
#else
				vOtherVelocity = pOther->GetAbsVelocity();
#endif
			}
		}
		else if ( pOther->IsPlayer() && pOther->VPhysicsGetObject() )
		{
			pOther->VPhysicsGetObject()->GetVelocity( &vOtherVelocity, NULL );

			if ( vOtherVelocity == vec3_origin )
			{
				vOtherVelocity = pOther->GetAbsVelocity();
			}
		}
		else
		{
#if defined( GAME_DLL )
			pOther->GetVelocity( &vOtherVelocity );
#else
			vOtherVelocity = pOther->GetAbsVelocity();
#endif
		}

		if( vOtherVelocity == vec3_origin )
		{
			// Recorded velocity is sometimes zero under pushed or teleported movement, or after position correction.
			// In these circumstances, we want implicit velocity ((last pos - this pos) / timestep )
			if ( pOtherPhysObject )
			{
				Vector vOtherImplicitVelocity;
				pOtherPhysObject->GetImplicitVelocity( &vOtherImplicitVelocity, NULL );
				vOtherVelocity += vOtherImplicitVelocity;
			}
		}
	}

	return vOtherVelocity;
}


bool CPortal_Base2D::ShouldTeleportTouchingEntity( CBaseEntity *pOther )
{
	if( m_hLinkedPortal.Get() == NULL )
	{
#if defined( GAME_DLL )
#if !defined ( DISABLE_DEBUG_HISTORY )
		if ( !IsMarkedForDeletion() )
		{
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it has no linked partner portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
		}
#endif
		if ( sv_portal_debug_touch.GetBool() )
		{
			Msg( "Portal %i not teleporting %s because it has no linked partner portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
		}
#endif
		return false;
	}

	bool bMobilePortals = IsMobile() || m_hLinkedPortal->IsMobile();
	if( bMobilePortals && !sv_allow_mobile_portal_teleportation.GetBool() )
		return false;

	//can't teleport an entity we don't own, unless this is a mobile portal interaction
	if( !m_PortalSimulator.OwnsEntity(pOther) && !bMobilePortals )
	{
#if defined( GAME_DLL )
#if !defined ( DISABLE_DEBUG_HISTORY )
		if ( !IsMarkedForDeletion() )
		{
			ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it's not simulated by this portal.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
		}
#endif
		if ( sv_portal_debug_touch.GetBool() )
		{
			Msg( "Portal %i not teleporting %s because it's not simulated by this portal. : %f \n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName(), gpGlobals->curtime );
		}
#endif
		return false;
	}



	if( !CPortal_Base2D_Shared::IsEntityTeleportable( pOther ) )
		return false;

	//Vector ptOtherOrigin = pOther->GetAbsOrigin();
	Vector ptOtherCenter = pOther->WorldSpaceCenter();
	Vector vOtherVelocity = Portal_FindUsefulVelocity( pOther );
	vOtherVelocity -= GetAbsVelocity(); //subtract the portal's velocity if it's moving. It's all relative.

	if( vOtherVelocity.Dot( m_PortalSimulator.GetInternalData().Placement.vForward ) > 0.0f )
	{
		return false;
	}

	if( bMobilePortals )
	{
		return pOther->IsPlayer() && (vOtherVelocity.Dot( m_PortalSimulator.GetInternalData().Placement.vForward ) < -5.0f); //only allow players for now in mobile portal teleportations
	}

	// Test for entity's center being past portal plane
	if(m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Normal.Dot( ptOtherCenter ) < m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Dist)
	{
		//entity wants to go further into the plane
		if( m_PortalSimulator.EntityIsInPortalHole( pOther ) )
		{
#ifdef _DEBUG
			static int iAntiRecurse = 0;
			if( pOther->IsPlayer() && (iAntiRecurse == 0) )
			{
				++iAntiRecurse;
				ShouldTeleportTouchingEntity( pOther ); //do it again for debugging
				--iAntiRecurse;
			}
#endif
			return true;
		}
#if defined( GAME_DLL )
		else
		{
#if !defined ( DISABLE_DEBUG_HISTORY )
			if ( !IsMarkedForDeletion() )
			{
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i not teleporting %s because it was not in the portal hole.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() ) );
			}
#endif
			if ( sv_portal_debug_touch.GetBool() )
			{
				Msg( "Portal %i not teleporting %s because it was not in the portal hole.\n", ((m_bIsPortal2)?(2):(1)), pOther->GetDebugName() );
			}
		}
#endif
	}

	return false;
}

void CPortal_Base2D::TeleportTouchingEntity( CBaseEntity *pOther )
{
	if ( GetPortalCallQueue() )
	{
		GetPortalCallQueue()->QueueCall( this, &CPortal_Base2D::TeleportTouchingEntity, pOther );
		return;
	}

	PreTeleportTouchingEntity( pOther );

	bool bMobilePortals = IsMobile() || m_hLinkedPortal->IsMobile();

	Assert( m_hLinkedPortal.Get() != NULL );

	Vector ptOtherOrigin = pOther->GetAbsOrigin();
	Vector ptOtherCenter;

	bool bPlayer = pOther->IsPlayer();
	QAngle qPlayerEyeAngles;
	CPortal_Player *pOtherAsPlayer;


	if( bPlayer )
	{
		//NDebugOverlay::EntityBounds( pOther, 255, 0, 0, 128, 60.0f );
		pOtherAsPlayer = (CPortal_Player *)pOther;
		qPlayerEyeAngles = pOtherAsPlayer->pl.v_angle;
		Warning( "PORTALLING PLAYER SHOULD BE DONE IN GAMEMOVEMENT\n" );
	}
	else
	{
		pOtherAsPlayer = NULL;
	}

	ptOtherCenter = pOther->WorldSpaceCenter();

	bool bNonPhysical = false; //special case handling for non-physical objects such as the energy ball and player



	QAngle qOtherAngles;
	Vector vOtherVelocity;

	vOtherVelocity = Portal_FindUsefulVelocity( pOther );
	vOtherVelocity -= GetAbsVelocity(); //subtract the portal's velocity if it's moving. It's all relative.

	const PS_InternalData_t &RemotePortalDataAccess = m_hLinkedPortal->m_PortalSimulator.GetInternalData();
	const PS_InternalData_t &LocalPortalDataAccess = m_PortalSimulator.GetInternalData();

#if defined( GAME_DLL )
	bool bCrouchPlayer = false;
#endif
	if( bPlayer )
	{
		qOtherAngles = pOtherAsPlayer->EyeAngles();
		bNonPhysical = true;
		Vector vWorldUp( 0.0f, 0.0f, 1.0f );
		vWorldUp = m_matrixThisToLinked.ApplyRotation( vWorldUp );

		if( fabs( vWorldUp.z ) < 0.7071f ) //the transformation will change our notion of UP significantly
		{
			//we have to compensate for the fact that AABB's don't rotate ever			
			pOtherAsPlayer->SetGroundEntity( NULL );

			//curl the player up into a little ball
#if defined( GAME_DLL )
			if( !pOtherAsPlayer->IsDucked() )
			{
				bCrouchPlayer = true;
				ptOtherOrigin.z += 18.0f;
				//pOtherAsPlayer->ForceDuckThisFrame(); NOTE: This should never be used! Players teleport in game movement!!!
			}
#endif
		}
	}
	else
	{
		qOtherAngles = pOther->GetAbsAngles();
		bNonPhysical = FClassnameIs( pOther, "prop_energy_ball" );
	}

	if( bMobilePortals ) //hack the position a bit so it pops out as far in front of the remote portal as it is in front of the local portal
	{
		float fPlaneDist = LocalPortalDataAccess.Placement.PortalPlane.DistTo( ptOtherCenter );
		Vector vOffset = LocalPortalDataAccess.Placement.vForward * (-(fPlaneDist * 2.0f)); //equidistant from the portal plane, but on the opposite side
		ptOtherCenter += vOffset;
		ptOtherOrigin += vOffset;
		//NDebugOverlay::Box( ptOtherCenter, Vector( -10.f, -10.f, -10.0f ), Vector( 10.0f, 10.0f, 10.0f ), 0, 255, 0, 128, 10.0f );
	}


	Vector ptNewOrigin;
	QAngle qNewAngles;
	Vector vNewVelocity;
	//apply transforms to relevant variables (applied to the entity later)
	{
		if( bPlayer )
		{
			ptNewOrigin = m_matrixThisToLinked * ptOtherCenter;
			ptNewOrigin += ( ptOtherOrigin - ptOtherCenter );

			// TODO: For accuracy sake we should handle this for all portal orientations
			// It's only abusable for floor to floor though.
			if ( IsFloorPortal( 0.9f ) && m_hLinkedPortal->IsFloorPortal( 0.9f ) )
			{
				// When we teleported we were assuming that our center was exactly on the same plane as the portal
				// We need to see how far off we were from that, Then we need to *double* compensate to push us
				// exactly as far off in our new position as we were before.
				ptNewOrigin.z -= 2.0f*( GetAbsOrigin() - ptOtherCenter ).z;
			}
		}
		else
		{
			ptNewOrigin = m_matrixThisToLinked * ptOtherOrigin;
		}

		// Reorient object angles, originally we did a transformation on the angles, but that doesn't quite work right for gimbal lock cases
		qNewAngles = TransformAnglesToWorldSpace( qOtherAngles, m_matrixThisToLinked.As3x4() );

		qNewAngles.x = AngleNormalizePositive( qNewAngles.x );
		qNewAngles.y = AngleNormalizePositive( qNewAngles.y );
		qNewAngles.z = AngleNormalizePositive( qNewAngles.z );

		QAngle savedAngles = qNewAngles;

		// Our teleport is going to roll us. See if we could find an over extended angle that would prevent a roll since that's the most disorienting thing.
		if( bPlayer && qNewAngles[ROLL] != 0.0f )
		{
			float bestAngle = fabs( 180.f - qNewAngles.z );
			float punchMag = 0.0f;
			QAngle punchAngles;

			// try to adjust the initial angle to see if we can get something better
			for ( int i = 0; i < 20; i++ )
			{
				QAngle qTestAngles = qOtherAngles;
				float pitchAdjust = 5.f*(1 + i/2);
				if( i%2 == 1 )
				{
					pitchAdjust *= -1.f;
				}
				qTestAngles[PITCH] += pitchAdjust;

				qTestAngles = TransformAnglesToWorldSpace( qTestAngles, m_matrixThisToLinked.As3x4() );

				qTestAngles.x = AngleNormalizePositive( qTestAngles.x );
				qTestAngles.y = AngleNormalizePositive( qTestAngles.y );
				qTestAngles.z = AngleNormalizePositive( qTestAngles.z );


				if( fabs( 180.f - qTestAngles.z ) > (bestAngle + 1.1f*fabs(pitchAdjust)) )
				{
					bestAngle = fabs( 180.f - qTestAngles.z );
					qNewAngles = qTestAngles;
					punchMag = pitchAdjust;
				}
			}

			punchAngles = savedAngles - qNewAngles;

			if( punchAngles.y < 180.f )
				punchAngles.y += 360.f;
			if( punchAngles.y > 180.f )
				punchAngles.y -= 360.f;

			if( fabs( punchAngles.y ) > 120.f )
			{
				punchAngles.z = 0.f;
				punchAngles.y = 0.f;
				punchAngles.x = -punchMag;
			}

			pOtherAsPlayer->SetPunchAngle( punchAngles );
		}

		// Reorient the velocity		
		vNewVelocity = m_matrixThisToLinked.ApplyRotation( vOtherVelocity );
	}

	//velocity hacks
	{
		float fExitMin, fExitMax;
		CPortal_Base2D::GetExitSpeedRange( this, bPlayer, fExitMin, fExitMax, ptNewOrigin, pOther );

		float fSpeed = vNewVelocity.Length();
		if( fSpeed == 0.0f )
		{
			if( fExitMin >= 0.0f )
			{
				vNewVelocity = m_hLinkedPortal->m_vForward * fExitMin;
			}
		}
		else
		{
			if( fSpeed < fExitMin )
			{
				vNewVelocity *= (fExitMin / fSpeed);
			}
			else if( fSpeed > fExitMax )
			{
				vNewVelocity *= (fExitMax / fSpeed);
			}
		}

		if( bPlayer )
		{
			bool bPitchReorientation = false;
			if ( IsFloorPortal( 0.9f ) && m_hLinkedPortal->IsFloorPortal( 0.9f ) ) //floor to floor transition
			{
				Vector vPlayerEyeForward;
				pOtherAsPlayer->EyeVectors( &vPlayerEyeForward );
				if( vPlayerEyeForward.z > fabs( 0.85f ) ) //player looking mostly down into entrance portal
				{
					//do a pitch reorientation instead of a corkscrew roll
					bPitchReorientation = true;
				}

				const float cfBowieManueverThreshold = sv_bowie_maneuver_threshold.GetFloat();
				if( vOtherVelocity.LengthSqr() < (cfBowieManueverThreshold * cfBowieManueverThreshold) ) //velocity below the threshold where we'd like to do the David Bowie move from Labyrinth where he was walking around the crazy Escher style room 
				{
					//compute what direction we'll be facing coming out of the portal, but projected onto the exit portal plane and normalized
					Vector vPlayerForward;
					pOtherAsPlayer->GetVectors( &vPlayerForward, NULL, NULL );					
					Vector vBowieForward = LocalPortalDataAccess.Placement.matThisToLinked.ApplyRotation( vPlayerForward );
					vBowieForward = vBowieForward - (RemotePortalDataAccess.Placement.vForward * vBowieForward.Dot( RemotePortalDataAccess.Placement.vForward )); //project onto the exit portal plane
					vBowieForward.NormalizeInPlace();

					// negate our forward since we are always going to be facing the wrong way when we come out.
					vBowieForward = -vBowieForward;

					//figure out which axis our forward is most aligned with, then base magnitude on how far we'd have to go to reach the portal border on that side
					float fGoToSide = fabs( vBowieForward.Dot( RemotePortalDataAccess.Placement.vRight ) );
					float fGoToBottom = fabs( vBowieForward.Dot( -RemotePortalDataAccess.Placement.vUp ) );
					float fPushMagnitude;
					if( fGoToSide > fGoToBottom )
					{
						fPushMagnitude = RemotePortalDataAccess.Placement.fHalfWidth / fGoToSide;
					}
					else
					{
						fPushMagnitude = RemotePortalDataAccess.Placement.fHalfHeight / fGoToBottom;
					}

					//calculate existing velocity perpendicular to the exit plane
					Vector vExistingPlanarVelocity = vNewVelocity - RemotePortalDataAccess.Placement.vForward * vNewVelocity.Dot( RemotePortalDataAccess.Placement.vForward );

					// if the Bowie maneuver velocity would be greater than our existing coplanar velocity, cancel out the existing coplanar velocity and add in the Bowie maneuver velocity
					//					if( 2.0f*(fPushMagnitude * fPushMagnitude) > vExistingPlanarVelocity.LengthSqr() ) // we really prefer to bowie, force this for now.
					{
						// We don't want to keep spinning - give us an extra 20% to make sure we get out.
						Vector vecAdded = 1.2f*((vBowieForward * fPushMagnitude) - vExistingPlanarVelocity);
						vNewVelocity += vecAdded;
					}
				}				
			}

			pOtherAsPlayer->m_bPitchReorientation = bPitchReorientation;
		}
	}

#if defined( GAME_DLL )
	//both changing portal environment and teleportation can trigger penetration solving. Disable solving while we're performing the migration and teleportations as we'll be in a bad state until both complete.
	CPortal_CollisionEvent::DisablePenetrationSolving_Push( true );
#endif

	//untouch the portal(s), will force a touch on destination after the teleport
	if( !bMobilePortals )
	{
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther, true );

		m_hLinkedPortal->m_PortalSimulator.TakeOwnershipOfEntity( pOther );

		//m_hLinkedPortal->PhysicsNotifyOtherOfUntouch( m_hLinkedPortal, pOther );
		//pOther->PhysicsNotifyOtherOfUntouch( pOther, m_hLinkedPortal );
	}

#if defined( GAME_DLL )
	if( sv_portal_debug_touch.GetBool() )
	{
		DevMsg( "===PORTAL %i TELEPORTING: %s : %f %f %f : %f===\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), vOtherVelocity.x, vOtherVelocity.y, vOtherVelocity.z, gpGlobals->curtime );
	}
#if !defined ( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "PORTAL %i TELEPORTING: %s\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname() ) );
	}
#endif
#endif

	//do the actual teleportation
	{
		pOther->SetGroundEntity( NULL );

		if( bPlayer )
		{
			QAngle qTransformedEyeAngles = TransformAnglesToWorldSpace( qPlayerEyeAngles, m_matrixThisToLinked.As3x4() );
			qTransformedEyeAngles.x = AngleNormalizePositive( qTransformedEyeAngles.x );
			qTransformedEyeAngles.y = AngleNormalizePositive( qTransformedEyeAngles.y );
			qTransformedEyeAngles.z = AngleNormalizePositive( qTransformedEyeAngles.z );

			pOtherAsPlayer->pl.v_angle = qTransformedEyeAngles;

#if defined( GAME_DLL )
			pOtherAsPlayer->pl.fixangle = FIXANGLE_ABSOLUTE;
			pOtherAsPlayer->UpdateVPhysicsPosition( ptNewOrigin, vNewVelocity, 0.0f );
			pOtherAsPlayer->Teleport( &ptNewOrigin, &qNewAngles, &vNewVelocity );
#else
			pOtherAsPlayer->SetAbsOrigin( ptNewOrigin );
			pOtherAsPlayer->SetAbsAngles( qNewAngles );
			pOtherAsPlayer->SetAbsVelocity( vNewVelocity );
#endif

			
			//pOtherAsPlayer->UnDuck();

			//pOtherAsPlayer->m_angEyeAngles = qTransformedEyeAngles;
			//pOtherAsPlayer->pl.v_angle = qTransformedEyeAngles;
			//pOtherAsPlayer->pl.fixangle = FIXANGLE_ABSOLUTE;
		}
		else
		{
			if( bNonPhysical )
			{
#if defined( GAME_DLL )
				pOther->Teleport( &ptNewOrigin, &qNewAngles, &vNewVelocity );
#else
				pOther->SetAbsOrigin( ptNewOrigin );
				pOther->SetAbsAngles( qNewAngles );
				pOther->SetAbsVelocity( vNewVelocity );
#endif
			}
			else
			{
				//doing velocity in two stages as a bug workaround, setting the velocity to anything other than 0 will screw up how objects rest on this entity in the future
#if defined( GAME_DLL )
				pOther->Teleport( &ptNewOrigin, &qNewAngles, &vec3_origin );
#else
				pOther->SetAbsOrigin( ptNewOrigin );
				pOther->SetAbsAngles( qNewAngles );
				pOther->SetAbsVelocity( vec3_origin );
#endif
				// Hacks for the Wheatley battle.  We need the futbols to be portalled in an intuitive way.
				if( FClassnameIs( pOther, "prop_exploding_futbol" ) )
				{
					// Exiting a floor portal: Always come out headed towards the top of the exit portal
					if( m_hLinkedPortal->m_vForward.z == 1.f )
					{
						vNewVelocity = m_hLinkedPortal->m_vUp * vNewVelocity.Length();
						
						// Build our pitch matrix
						Vector vRotAxis = m_hLinkedPortal->m_vRight;
						VMatrix mtxRotation;
						float flRotationAmt = -sv_futbol_floor_exit_angle.GetFloat();
						MatrixBuildRotationAboutAxis( mtxRotation, vRotAxis, flRotationAmt );

						vNewVelocity = mtxRotation * vNewVelocity;
					}
					// Exiting a wall portal: Shoot straight out
					else
					{
						// Magic number so the bombs will fly the same speed everytime
						vNewVelocity = m_hLinkedPortal->m_vForward * 850.f;
					}

				}

				pOther->ApplyAbsVelocityImpulse( vNewVelocity );
				
			}
		}

		pOther->RemoveEffects( EF_NOINTERP );
	}

#if defined( GAME_DLL )
	CPortal_CollisionEvent::DisablePenetrationSolving_Pop(); //re-enable penetration solving now that we're in the target environment and at the target position
	if( sv_portal_teleportation_resets_collision_events.GetBool() )
	{
		g_Collisions.RemovePenetrationEvents( pOther );
	}
#endif

	if( bMobilePortals )
	{
		FindClosestPassableSpace( pOther, LocalPortalDataAccess.Placement.vForward );
	}

#if defined( GAME_DLL )
	IPhysicsObject *pPhys = pOther->VPhysicsGetObject();
	if( (pPhys != NULL) && (pPhys->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
	{
		CPortal_Player *pHoldingPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pOther );
		pHoldingPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
		if ( pHoldingPlayer->IsHeldObjectOnOppositeSideOfPortal() )
		{
			pHoldingPlayer->SetHeldObjectPortal( this );
		}
		else
		{
			pHoldingPlayer->SetHeldObjectPortal( NULL );
		}

		CGrabController *pController = GetGrabControllerForPlayer( pHoldingPlayer );
		if( pController )
		{
			pController->CheckPortalOscillation( this, pOther, pHoldingPlayer );
		}

		for ( int i = 0; i < IProjectedWallEntityAutoList::AutoList().Count(); ++i )
		{
			CProjectedWallEntity *pWall = static_cast< CProjectedWallEntity* >( IProjectedWallEntityAutoList::AutoList()[i] );
			pWall->DisplaceObstructingEntity( pOther, true );
		}

		// For alternate ticks, if we're going to simulate physics again make sure the grab controller's target position is up to date with the
		// teleported object position.
		if ( PhysIsFinalTick() == false )
		{
			UpdateGrabControllerTargetPosition( pHoldingPlayer, NULL, NULL, true );
		}
	}
	else if( bPlayer )
	{
		CBaseEntity *pHeldEntity = GetPlayerHeldEntity( pOtherAsPlayer );
		if( pHeldEntity )
		{
			pOtherAsPlayer->ToggleHeldObjectOnOppositeSideOfPortal();
			if( pOtherAsPlayer->IsHeldObjectOnOppositeSideOfPortal() )
			{
				pOtherAsPlayer->SetHeldObjectPortal( m_hLinkedPortal );
			}
			else
			{
				pOtherAsPlayer->SetHeldObjectPortal( NULL );

				//we need to make sure the held object and player don't interpenetrate when the player's shape changes
				Vector vTargetPosition;
				QAngle qTargetOrientation;
				UpdateGrabControllerTargetPosition( pOtherAsPlayer, &vTargetPosition, &qTargetOrientation );

				pHeldEntity->Teleport( &vTargetPosition, &qTargetOrientation, 0 );

				FindClosestPassableSpace( pHeldEntity, RemotePortalDataAccess.Placement.vForward );
			}
		}

		//we haven't found a good way of fixing the problem of "how do you reorient an AABB". So we just move the player so that they fit
		m_hLinkedPortal->ForceEntityToFitInPortalWall( pOtherAsPlayer );
	}
#endif

	//force the entity to be touching the other portal right this millisecond
	if( !bMobilePortals )
	{
		trace_t Trace;
		memset( &Trace, 0, sizeof(trace_t) );
		//UTIL_TraceEntity( pOther, ptNewOrigin, ptNewOrigin, MASK_SOLID, pOther, COLLISION_GROUP_NONE, &Trace ); //fires off some asserts, and we just need a dummy anyways

		pOther->PhysicsMarkEntitiesAsTouching( m_hLinkedPortal.Get(), Trace );
		m_hLinkedPortal.Get()->PhysicsMarkEntitiesAsTouching( pOther, Trace );
	}

#if defined( GAME_DLL )
	// Notify the entity that it's being teleported
	// Tell the teleported entity of the portal it has just arrived at
	notify_teleport_params_t paramsTeleport;
	paramsTeleport.prevOrigin		= ptOtherOrigin;
	paramsTeleport.prevAngles		= qOtherAngles;
	paramsTeleport.physicsRotate	= true;
	notify_system_event_params_t eventParams ( &paramsTeleport );
	pOther->NotifySystemEvent( this, NOTIFY_EVENT_TELEPORT, eventParams );

	// Notify the portals to fire appropriate outputs
	OnEntityTeleportedFromPortal( pOther );
	if ( m_hLinkedPortal )
	{
		m_hLinkedPortal->OnEntityTeleportedToPortal( pOther );
	}

	//notify clients of the teleportation
	EntityPortalled( this, pOther, ptNewOrigin, qNewAngles, false );
#endif

#ifdef _DEBUG
	{
		Vector ptTestCenter = pOther->WorldSpaceCenter();

		float fNewDist, fOldDist;
		fNewDist = RemotePortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptTestCenter ) - RemotePortalDataAccess.Placement.PortalPlane.m_Dist;
		fOldDist = LocalPortalDataAccess.Placement.PortalPlane.m_Normal.Dot( ptOtherCenter ) - LocalPortalDataAccess.Placement.PortalPlane.m_Dist;
		AssertMsg( fNewDist >= 0.0f, "Entity portalled behind the destination portal." );
	}
#endif

#if defined( GAME_DLL )
	pOther->NetworkProp()->NetworkStateForceUpdate();
	if( bPlayer )
		pOtherAsPlayer->pl.NetworkStateChanged();
#endif

	//if( bPlayer )
	//	NDebugOverlay::EntityBounds( pOther, 0, 255, 0, 128, 60.0f );

	Assert( (bPlayer == false) || (pOtherAsPlayer->m_hPortalEnvironment.Get() == m_hLinkedPortal.Get()) );

	PostTeleportTouchingEntity( pOther );

#if defined( CLIENT_DLL ) && 0 //debugging code
	{
		Vector vPos = pOther->GetAbsOrigin();
		Warning( "CPortal_Base2D::TeleportTouchingEntity(%s), portal %d, time %f %f, End Position: %f %f %f\n", pOther->GetClassname(), m_bIsPortal2 ? 2 : 1, gpGlobals->curtime, prediction->GetSavedTime(), vPos.x, vPos.y, vPos.z );
		NDebugOverlay::EntityBounds( pOther, 0, 0, 255, 100, 5.0f );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Sets the portal active
//-----------------------------------------------------------------------------
void CPortal_Base2D::SetActive( bool bActive )
{
	m_bOldActivatedState = m_bActivated;
	m_bActivated = bActive;
}

bool CPortal_Base2D::IsActivedAndLinked( void ) const
{
	return ( IsActive() && (m_hLinkedPortal.Get() != NULL) && (m_hLinkedPortal.Get()->IsActive()) );
}

bool CPortal_Base2D::IsFloorPortal( float fThreshold ) const
{
	return m_PortalSimulator.GetInternalData().Placement.vForward.z > fThreshold;
}

bool CPortal_Base2D::IsCeilingPortal( float fThreshold ) const
{
	return m_PortalSimulator.GetInternalData().Placement.vForward.z < fThreshold;
}

void CPortal_Base2D::PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity )
{
	if( pEntity->IsPlayer() )
	{
		//DevMsg( "Portal %i simulator took ownership of player: %f\n", ((m_bIsPortal2)?(2):(1)), gpGlobals->curtime );
		((CPortal_Player *)pEntity)->m_hPortalEnvironment = this;
	}
}

void CPortal_Base2D::PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity )
{
	if( pEntity->IsPlayer() && (((CPortal_Player *)pEntity)->m_hPortalEnvironment.Get() == this) )
	{
		//DevMsg( "Portal %i simulator released ownership of player: %f\n", ((m_bIsPortal2)?(2):(1)), gpGlobals->curtime );
		((CPortal_Player *)pEntity)->m_hPortalEnvironment = NULL;
	}
}




void CPortal_Base2D::UpdateCollisionShape( void )
{
	if( m_pCollisionShape )
	{
		physcollision->DestroyCollide( m_pCollisionShape );
		m_pCollisionShape = NULL;
	}

	Vector vLocalMins = GetLocalMins();
	Vector vLocalMaxs = GetLocalMaxs();

	if( (vLocalMaxs.x <= vLocalMins.x) || (vLocalMaxs.y <= vLocalMins.y) || (vLocalMaxs.z <= vLocalMins.z) )
		return; //volume is 0 (or less)


	//create the collision shape.... TODO: consider having one shared collideable between all portals
	float fPlanes[6*4];
	fPlanes[(0*4) + 0] = 1.0f;
	fPlanes[(0*4) + 1] = 0.0f;
	fPlanes[(0*4) + 2] = 0.0f;
	fPlanes[(0*4) + 3] = vLocalMaxs.x;

	fPlanes[(1*4) + 0] = -1.0f;
	fPlanes[(1*4) + 1] = 0.0f;
	fPlanes[(1*4) + 2] = 0.0f;
	fPlanes[(1*4) + 3] = -vLocalMins.x;

	fPlanes[(2*4) + 0] = 0.0f;
	fPlanes[(2*4) + 1] = 1.0f;
	fPlanes[(2*4) + 2] = 0.0f;
	fPlanes[(2*4) + 3] = vLocalMaxs.y;

	fPlanes[(3*4) + 0] = 0.0f;
	fPlanes[(3*4) + 1] = -1.0f;
	fPlanes[(3*4) + 2] = 0.0f;
	fPlanes[(3*4) + 3] = -vLocalMins.y;

	fPlanes[(4*4) + 0] = 0.0f;
	fPlanes[(4*4) + 1] = 0.0f;
	fPlanes[(4*4) + 2] = 1.0f;
	fPlanes[(4*4) + 3] = vLocalMaxs.z;

	fPlanes[(5*4) + 0] = 0.0f;
	fPlanes[(5*4) + 1] = 0.0f;
	fPlanes[(5*4) + 2] = -1.0f;
	fPlanes[(5*4) + 3] = -vLocalMins.z;

	CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, 0.00001f, true );
	Assert( pPolyhedron != NULL );
	CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
	pPolyhedron->Release();
	Assert( pConvex != NULL );
	m_pCollisionShape = physcollision->ConvertConvexToCollide( &pConvex, 1 );
}


float CPortal_Base2D::GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	return -FLT_MAX; //default behavior is to not mess with the speed
}

float CPortal_Base2D::GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	return FLT_MAX; //default behavior is to not mess with the speed
}


void CPortal_Base2D::GetExitSpeedRange( CPortal_Base2D *pEntrancePortal, bool bPlayer, float &fExitMinimum, float &fExitMaximum, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	CPortal_Base2D *pExitPortal = pEntrancePortal ? pEntrancePortal->m_hLinkedPortal.Get() : NULL;
	if( !pExitPortal )
	{
		fExitMinimum = -FLT_MAX;
		fExitMaximum = FLT_MAX;
		return;
	}
		
	const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians
	bool bEntranceOnFloor = pEntrancePortal->m_plane_Origin.normal.z > COS_PI_OVER_SIX;
	bool bExitOnFloor = pExitPortal->m_plane_Origin.normal.z > COS_PI_OVER_SIX;

	fExitMinimum = pExitPortal->GetMinimumExitSpeed( bPlayer, bEntranceOnFloor, bExitOnFloor, vEntityCenterAtExit, pEntity );
	fExitMaximum = pExitPortal->GetMaximumExitSpeed( bPlayer, bEntranceOnFloor, bExitOnFloor, vEntityCenterAtExit, pEntity );
}


