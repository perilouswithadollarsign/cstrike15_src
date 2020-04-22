//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Special handling for Portal usable ladders
//
//=============================================================================//
#include "cbase.h"
#include "portal_gamemovement.h"
#include "in_buttons.h"
#include "utlrbtree.h"
#include "movevars_shared.h"
#include "portal_shareddefs.h"
#include "portal_collideable_enumerator.h"
#include "portal_base2d_shared.h"
#include "rumble_shared.h"
#include "portal_mp_gamerules.h"
#include "tier0/stacktools.h"
#include "portal_util_shared.h"
#include "iclient.h"

#if defined( CLIENT_DLL )
	#include "c_portal_player.h"
	#include "c_rumble.h"
	#include "prediction.h"
	#include "c_weapon_portalgun.h"
	#include "c_projectedwallentity.h"
	#define CRecipientFilter C_RecipientFilter
#else
	#include "portal_player.h"
	#include "env_player_surface_trigger.h"
	#include "portal_gamestats.h"
	#include "physicsshadowclone.h"
	#include "recipientfilter.h"
	#include "SoundEmitterSystem/isoundemittersystembase.h"
	#include "weapon_portalgun.h"
	#include "projectedwallentity.h"
	#include "paint_power_info.h"
	#include "particle_parse.h"
#endif

#include "coordsize.h" // for DIST_EPSILON

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_player_trace_through_portals("sv_player_trace_through_portals", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Causes player movement traces to trace through portals." );
ConVar sv_player_funnel_into_portals("sv_player_funnel_into_portals", "1", FCVAR_REPLICATED, "Causes the player to auto correct toward the center of floor portals." ); 
ConVar sv_player_funnel_snap_threshold("sv_player_funnel_snap_threshold", "10.f", FCVAR_REPLICATED);
ConVar sv_player_funnel_speed_bonus("sv_player_funnel_speed_bonus", "2.f", FCVAR_REPLICATED | FCVAR_CHEAT);
ConVar sv_player_well_above_height("sv_player_funnel_well_above", "256.f", FCVAR_REPLICATED);
ConVar sv_player_funnel_height_adjust("sv_player_funnel_height_adjust", "128.f", FCVAR_REPLICATED);
ConVar sv_player_funnel_gimme_dot("sv_player_funnel_gimme_dot", "0.9", FCVAR_REPLICATED);

// Convars for paint powerups
ConVar sv_speed_normal("sv_speed_normal", "175.f", FCVAR_REPLICATED | FCVAR_CHEAT, "For tweaking the normal speed when off speed paint.");
ConVar sv_speed_paint_max("sv_speed_paint_max", "800.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "For tweaking the max speed for speed paint.");
ConVar sv_speed_paint_side_move_factor("sv_speed_paint_side_move_factor", "0.5f", FCVAR_REPLICATED | FCVAR_CHEAT);
ConVar speed_funnelling_enabled("speed_funnelling_enabled", "1", FCVAR_REPLICATED, "Toggle whether the player is funneled into portals while running on speed paint.");
ConVar sv_paintairacceleration("sv_paintairacceleration", "5.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "Air acceleration in Paint");

ConVar eggbot_sink_speed("eggbot_sink_speed", "120.0f", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar ballbot_sink_speed("ballbot_sink_speed", "120.0f", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
ConVar coop_sink_speed_decay("coop_sink_speed_decay","0.02f", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

ConVar coop_impact_velocity_threshold("coop_impact_velocity_threshold", "250.0", FCVAR_REPLICATED | FCVAR_CHEAT );

#define sv_can_carry_both_guns			0	//extern ConVar sv_can_carry_both_guns;

ConVar sv_portal_new_player_trace( "sv_portal_new_player_trace", "1", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar portal_funnel_debug( "portal_funnel_debug", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

//DIST_EPSILON == 0.03125
ConVar portal_player_interaction_quadtest_epsilon( "portal_player_interaction_quadtest_epsilon", "-0.03125", FCVAR_REPLICATED | FCVAR_CHEAT );

#if defined( CLIENT_DLL )
ConVar cl_vertical_elevator_fix( "cl_vertical_elevator_fix", "1" );
#endif

class CReservePlayerSpot;

#define PORTAL_FUNNEL_AMOUNT 6.0f
#define	MAX_CLIP_PLANES	5

//#define DEBUG_FLINGS

// Roughly how often we want to update the info about the ground surface we're on.
// We don't need to do this very often.
#define CATEGORIZE_GROUND_SURFACE_INTERVAL			0.3f
#define CATEGORIZE_GROUND_SURFACE_TICK_INTERVAL   ( (int)( CATEGORIZE_GROUND_SURFACE_INTERVAL / TICK_INTERVAL ) )

#define CHECK_STUCK_INTERVAL			1.0f
#define CHECK_STUCK_TICK_INTERVAL		( (int)( CHECK_STUCK_INTERVAL / TICK_INTERVAL ) )

#define CHECK_STUCK_INTERVAL_SP			0.2f
#define CHECK_STUCK_TICK_INTERVAL_SP	( (int)( CHECK_STUCK_INTERVAL_SP / TICK_INTERVAL ) )

#define CHECK_LADDER_INTERVAL			0.2f
#define CHECK_LADDER_TICK_INTERVAL		( (int)( CHECK_LADDER_INTERVAL / TICK_INTERVAL ) )

#define	NUM_CROUCH_HINTS	3

#define CRITICAL_SLOPE 0.7f
#define PLAYER_FLING_HELPER_MIN_SPEED 200.0f

const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians

extern bool g_bMovementOptimizations;
extern bool g_bAllowForcePortalTrace;
extern bool g_bForcePortalTrace;


// Unnamed namespace is the C++ way of specifying static at file scope.
// It prevents this function from leaking out of this translation unit.
namespace
{		  

	inline void DoTrace( ITraceListData *pTraceListData, const Ray_t &ray, uint32 fMask, ITraceFilter *filter, trace_t *ptr, int *counter )
	{
		++*counter;

		if ( pTraceListData && pTraceListData->CanTraceRay(ray) )
		{
			enginetrace->TraceRayAgainstLeafAndEntityList( ray, pTraceListData, fMask, filter, ptr );
		}
		else
		{
			enginetrace->TraceRay( ray, fMask, filter, ptr );
		}
	}

} // Unnamed namespace


#if defined( DEBUG_FLINGS )
class CDebugCrouchOverlay : public CAutoGameSystemPerFrame
{
public:
	CDebugCrouchOverlay( void )
	{
		pPlayer = NULL;
		pPortal = NULL;
		vCenterNudge = vTeleportVel = vTeleportPos = vUnduckVel = vUnduckPos = vUnduckJumpPos = vUnduckJumpVel = vec3_origin;
		bWatchFlingVelocity = false;
	}

#if defined( CLIENT_DLL )
	virtual void PreRender ()
#else
	virtual void PreClientUpdate()
#endif
	{
		if( pPlayer )
		{
			Vector vDuckMins = pPlayer->GetDuckHullMins();
			Vector vDuckMaxs = pPlayer->GetDuckHullMaxs();
			Vector vDuckExtents = (vDuckMaxs - vDuckMins) * 0.5f;

			Vector vStandMins = pPlayer->GetStandHullMins();
			Vector vStandMaxs = pPlayer->GetStandHullMaxs();
			Vector vStandExtents = (vStandMaxs - vStandMins) * 0.5f;

			if( vTeleportPos != vec3_origin )
			{
				NDebugOverlay::Box( vTeleportPos, -vTeleportExtents, vTeleportExtents, 255, 0, 0, 100, 0.0f );
				NDebugOverlay::HorzArrow( vTeleportPos, vTeleportPos + vTeleportVel * 2.0f, 2.0f, 255, 0, 0, 100, true, 0.0f );

				if( vCenterNudge != vec3_origin )
				{
					NDebugOverlay::HorzArrow( vTeleportPos - vCenterNudge, vTeleportPos, 2.0f, 0, 255, 0, 100, true, 0.0f );
				}
			}

			if( vUnduckPos != vec3_origin )
			{
				NDebugOverlay::Box( vUnduckPos, -vStandExtents, vStandExtents, 0, 255, 0, 100, 0.0f );
				NDebugOverlay::HorzArrow( vUnduckPos, vUnduckPos + vUnduckVel * 2.0f, 2.0f, 0, 255, 0, 100, true, 0.0f );
			}

			if( vUnduckJumpPos != vec3_origin )
			{
				NDebugOverlay::Box( vUnduckJumpPos, -vStandExtents, vStandExtents, 0, 0, 255, 100, 0.0f );
				NDebugOverlay::HorzArrow( vUnduckJumpPos, vUnduckJumpPos + vUnduckJumpVel * 2.0f, 2.0f, 0, 0, 255, 100, true, 0.0f );
			}
		}

	}

	CPortal_Base2D *pPortal;
	CPortal_Player *pPlayer;

	Vector vTeleportPos;
	Vector vTeleportVel;
	Vector vTeleportExtents;
	Vector vCenterNudge;

	Vector vUnduckPos;
	Vector vUnduckVel;
	Vector vUnduckJumpPos;
	Vector vUnduckJumpVel;

	bool bWatchFlingVelocity;
};
static CDebugCrouchOverlay s_MovementDebug;
#endif


//trace that has special understanding of how to handle portals
CTrace_PlayerAABB_vs_Portals::CTrace_PlayerAABB_vs_Portals( void )
{
	UTIL_ClearTrace( *this );
	m_bContactedPortalTransitionRamp = false;
}

bool CTrace_PlayerAABB_vs_Portals::HitPortalRamp( const Vector &vUp )
{
	return sv_portal_new_player_trace.GetBool() && (m_bContactedPortalTransitionRamp && DidHit() && (plane.normal.Dot( vUp ) > 0.0f));
}



void PortalTracePlayerBBoxForGround( const Vector& start, const Vector& end, const Vector& minsSrc,
									const Vector& maxsSrc, IHandleEntity *player, unsigned int fMask,
									int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm, const Vector& vStickNormal );


static inline CBaseEntity *TranslateGroundEntity( CBaseEntity *pGroundEntity )
{
#ifndef CLIENT_DLL
	CPhysicsShadowClone *pClone = dynamic_cast<CPhysicsShadowClone *>(pGroundEntity);

	if( pClone && pClone->IsUntransformedClone() )
	{
		CBaseEntity *pSource = pClone->GetClonedEntity();

		if( pSource )
			return pSource;
	}
#endif //#ifndef CLIENT_DLL

	return pGroundEntity;
}

//should force player to crouch for this transition
bool ShouldPortalTransitionCrouch( const CPortal_Base2D *pEnterPortal )
{
	const float flUpDot = fabs( pEnterPortal->m_matrixThisToLinked.m[2][2] ); //How much does zUp still look like zUp after going through this portal

	return (flUpDot < COS_PI_OVER_SIX && (sv_portal_new_player_trace.GetBool() || (flUpDot >= EQUAL_EPSILON)));
}

//if player is already crouched, do NOT automatically uncrouch. You don't actually have to check that the player is exiting the portal, but we assume that's the intent
bool ShouldMaintainFlingAssistCrouch( const CPortal_Base2D *pExitPortal, const Vector &vExitVelocity )
{
	return (pExitPortal->m_vForward.z > 0.1f && pExitPortal->m_vForward.z < 0.9) && (vExitVelocity.z > 1.0f) && (vExitVelocity.Dot( pExitPortal->m_vForward ) > PLAYER_FLING_HELPER_MIN_SPEED);
}

#if defined( CLIENT_DLL )
void CPortalGameMovement::ClientVerticalElevatorFixes( CBasePlayer *pPlayer, CMoveData *pMove )
{
	//find root move parent of our ground entity
	CBaseEntity *pRootMoveParent = pPlayer->GetGroundEntity();
	while( pRootMoveParent )
	{
		C_BaseEntity *pTestParent = pRootMoveParent->GetMoveParent();
		if( !pTestParent )
			break;

		pRootMoveParent = pTestParent;
	}

	//if it's a C_BaseToggle (func_movelinear / func_door) then enable prediction if it chooses to
	bool bRootMoveParentIsLinearMovingBaseToggle = false;
	bool bAdjustedRootZ = false;
	if( pRootMoveParent && !pRootMoveParent->IsWorld() )
	{
		C_BaseToggle *pPredictableGroundEntity = dynamic_cast<C_BaseToggle *>(pRootMoveParent);
		if( pPredictableGroundEntity && (pPredictableGroundEntity->m_movementType == MOVE_TOGGLE_LINEAR) )
		{
			bRootMoveParentIsLinearMovingBaseToggle = true;
			if( !pPredictableGroundEntity->GetPredictable() )
			{
				pPredictableGroundEntity->SetPredictionEligible( true );
				pPredictableGroundEntity->m_hPredictionOwner = pPlayer;
			}
			else if( cl_vertical_elevator_fix.GetBool() )
			{
				Vector vNewOrigin = pPredictableGroundEntity->PredictPosition( player->PredictedServerTime() + TICK_INTERVAL );
				if( (vNewOrigin - pPredictableGroundEntity->GetLocalOrigin()).LengthSqr() > 0.01f )
				{
					bAdjustedRootZ = (vNewOrigin.z != pPredictableGroundEntity->GetLocalOrigin().z);
					pPredictableGroundEntity->SetLocalOrigin( vNewOrigin );

					//invalidate abs transforms for upcoming traces
					C_BaseEntity *pParent = pPlayer->GetGroundEntity();
					while( pParent )
					{
						pParent->AddEFlags( EFL_DIRTY_ABSTRANSFORM );
						pParent = pParent->GetMoveParent();
					}
				}
			}
		}
	}

	//re-seat player on vertical elevators
	if( bRootMoveParentIsLinearMovingBaseToggle && 
		cl_vertical_elevator_fix.GetBool() && 
		bAdjustedRootZ )
	{
		trace_t trElevator;
		TracePlayerBBox( pMove->GetAbsOrigin(), pMove->GetAbsOrigin() - Vector( 0.0f, 0.0f, GetPlayerMaxs().z ), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trElevator );

		if( trElevator.startsolid )
		{
			//started in solid, and we think it's an elevator. Pop up the player if at all possible

			//trace up, ignoring the ground entity hierarchy
			Ray_t playerRay;
			playerRay.Init( pMove->GetAbsOrigin(), pMove->GetAbsOrigin() + Vector( 0.0f, 0.0f, GetPlayerMaxs().z ), GetPlayerMins(), GetPlayerMaxs() );

			CTraceFilterSimpleList ignoreGroundEntityHeirarchy( COLLISION_GROUP_PLAYER_MOVEMENT );
			{
				ignoreGroundEntityHeirarchy.AddEntityToIgnore( pPlayer );
				C_BaseEntity *pParent = pPlayer->GetGroundEntity();
				while( pParent )
				{
					ignoreGroundEntityHeirarchy.AddEntityToIgnore( pParent );
					pParent = pParent->GetMoveParent();
				}
			}

			

			enginetrace->TraceRay( playerRay, MASK_PLAYERSOLID, &ignoreGroundEntityHeirarchy, &trElevator );
			if( !trElevator.startsolid ) //success
			{
				//now trace back down
				Vector vStart = trElevator.endpos;
				TracePlayerBBox( vStart, pMove->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, trElevator );
				if( !trElevator.startsolid &&
					(trElevator.m_pEnt == pPlayer->GetGroundEntity()) )
				{
					//if we landed back on the ground entity, call it good
					pMove->SetAbsOrigin( trElevator.endpos );
					pPlayer->SetNetworkOrigin( trElevator.endpos ); //paint code loads from network origin after handling paint powers
				}
			}
		}
		else if( (trElevator.endpos.z < pMove->GetAbsOrigin().z) && (trElevator.m_pEnt == pPlayer->GetGroundEntity()) )
		{
			//re-seat on ground entity
			pMove->SetAbsOrigin( trElevator.endpos );
			pPlayer->SetNetworkOrigin( trElevator.endpos ); //paint code loads from network origin after handling paint powers
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPortalGameMovement::CPortalGameMovement()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CPortal_Player* CPortalGameMovement::GetPortalPlayer() const
{
	return assert_cast< CPortal_Player* >( player );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMove - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove )
{
	AssertMsg( pPlayer && pMove, "Null pointers are bad, and you should feel bad." );
	if ( pPlayer && pMove )
	{
		float flStoreFrametime = gpGlobals->frametime;

		// Save the current paint player and move data
		player = pPlayer;
		mv = pMove;

#if defined( CLIENT_DLL )
		ClientVerticalElevatorFixes( pPlayer, pMove ); //fixup vertical elevator discrepancies between client and server as best we can
#endif

		m_vMoveStartPosition = mv->GetAbsOrigin();

		// Get the paint player
		CPortal_Player *pPlayer = GetPortalPlayer();

		//!!HACK HACK: Adrian - slow down all player movement by this factor.
		//!!Blame Yahn for this one.
		gpGlobals->frametime *= pPlayer->GetLaggedMovementValue();

		// Reset point contents for water check.
		ResetGetWaterContentsForPointCache();

		// Cropping movement speed scales mv->m_fForwardSpeed etc. globally
		// Once we crop, we don't want to recursively crop again, so we set the crop
		// flag globally here once per usercmd cycle.
		m_bSpeedCropped = false;

		m_bInPortalEnv = (((CPortal_Player *)pPlayer)->m_hPortalEnvironment != NULL);

		g_bAllowForcePortalTrace = m_bInPortalEnv;
		g_bForcePortalTrace = m_bInPortalEnv;

		// Figure out move direction
		const Vector& stickNormal = pPlayer->GetPortalPlayerLocalData().m_StickNormal;
		Vector vForward, vRight;
		Vector vPlayerUp = pPlayer->GetPortalPlayerLocalData().m_Up;
		AngleVectors( mv->m_vecViewAngles, &vForward, &vRight, NULL );  // Determine movement angles

		const Vector worldUp( 0, 0, 1 );
		bool shouldProjectInputVectorsOntoGround = pPlayer->GetGroundEntity() != NULL;

		if( shouldProjectInputVectorsOntoGround )
		{
			vForward -= DotProduct( vForward, stickNormal ) * stickNormal;
			vRight -= DotProduct( vRight, stickNormal ) * stickNormal;

			vForward.NormalizeInPlace();
			vRight.NormalizeInPlace();
		}

		Vector vWishVel = vForward*mv->m_flForwardMove + vRight*mv->m_flSideMove;
		vWishVel -= vPlayerUp * DotProduct( vWishVel, vPlayerUp );

		// Make sure the gravity direction is the negation of the current stick normal
		// Note: This is cached off before the powers are used only because, if stick
		//		 deactivates because of a jump, the jump direction must be the surface
		//		 (stick) normal.
		m_vGravityDirection = -stickNormal;

		// Figure out paint power
		pPlayer->SetInputVector( vWishVel );
		pPlayer->UpdatePaintPowers();

		// Using the paint power may change the velocity
		mv->m_vecVelocity = player->GetAbsVelocity();

		// The stick power may cause the abs origin and collision bounds to change
#if defined( GAME_DLL )
		mv->SetAbsOrigin( player->GetAbsOrigin() );
#else
		mv->SetAbsOrigin( player->GetNetworkOrigin() ); //GetAbsOrigin() introduces prediction delay
#endif

		// If the player is sticking this frame, make sure she stays on the surface
		// Note: Currently, the only case where stick is the power and the player is
		//		 not on the surface is when we expand the box to try to find sticky
		//		 surfaces immediately after teleporting.
		//StayStuckToSurface();

		// Use this player's max speed (dependent on whether he's on speed paint)
		const float maxSpeed = pPlayer->MaxSpeed();
		mv->m_flClientMaxSpeed = mv->m_flMaxSpeed = maxSpeed;

		// Run the command.
		PlayerMove();
		HandlePortalling();
		FinishMove();

		//if( gpGlobals->frametime != 0.0f )
		//{
		//	DevMsg( "Max Speed: %f\n", maxSpeed );
		//	DevMsg( "Speed: %f\n", mv->m_vecVelocity.Length() );
		//	DevMsg( "XY Speed: %f\n", mv->m_vecVelocity.Length2D() );
		//	DevMsg( "XY Player Speed: %f\n", player->GetLocalVelocity().Length2D() );
		//	DevMsg( "Eye Offset: (%f, %f, %f)\n", XYZ( pPlayer->GetViewOffset() ) );
		//	DevMsg( "Final Player Velocity: (%f, %f, %f)\n", XYZ( player->GetAbsVelocity() ) );
		//}

#ifndef CLIENT_DLL
		pPlayer->UnforceButtons( IN_DUCK );
		pPlayer->UnforceButtons( IN_JUMP );

		// Try to collide with thrown weapons if we don't currently have a weapon
		if( pPlayer->HasWeapons() == false || sv_can_carry_both_guns )
		{
			// Trace for the portal and paint guns
			trace_t pm;
			TracePlayerBBox( player->GetAbsOrigin(), player->GetAbsOrigin() + gpGlobals->frametime * player->GetAbsVelocity(), MASK_PLAYERSOLID, COLLISION_GROUP_WEAPON, pm );
			if ( pm.m_pEnt && ( FClassnameIs( pm.m_pEnt, "weapon_portalgun" ) || FClassnameIs( pm.m_pEnt, "weapon_paintgun" ) ) )
			{
				CBasePortalCombatWeapon *pWeap = dynamic_cast< CBasePortalCombatWeapon* >( pm.m_pEnt );

				// Only pick up the weapon if there's been enough time since it was thrown
				if ( pWeap && pWeap->EnoughTimeSinceThrown() )
				{
					pPlayer->BumpWeapon( pWeap );

					CWeaponPortalgun* pPortalgun = dynamic_cast< CWeaponPortalgun* >( pWeap );
					unsigned char iPortalLinkage = 0;
					if( pPortalgun )
					{
						iPortalLinkage = pPortalgun->GetLinkageGroupID();
					}

					pWeap->OnPickedUp( pPlayer );	// We bump into weapons instead of weapons bumping into us, so call OnPickedUp on the gun's behalf

					if( pPortalgun )
					{
						pPortalgun->SetLinkageGroupID( iPortalLinkage );
					}
				}
			}
		}
#endif

		//This is probably not needed, but just in case.
		gpGlobals->frametime = flStoreFrametime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Base jump behavior, plus an anim event
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPortalGameMovement::CheckJumpButton()
{
	if (player->pl.deadflag)
		return false;

	// Cannot jump will ducked.
	if ( player->GetFlags() & FL_DUCKING )
		return false;

	// No more effect
	if (player->GetGroundEntity() == NULL || player->GetGroundEntity()->IsPlayer() )
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	if ( mv->m_nOldButtons & IN_JUMP )
		return false;		// don't pogo stick

	// Cannot jump will in the unduck transition.
	if ( player->m_Local.m_bDucking && (  player->GetFlags() & FL_DUCKING ) )
		return false;

	// Still updating the eye position.
	if ( player->m_Local.m_nDuckJumpTimeMsecs > 0 )
		return false;


	// Start jump animation and player sound (specific TF animation and flags).
	GetPortalPlayer()->DoAnimationEvent( PLAYERANIMEVENT_JUMP, 0 );
	player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );

	// In the air now.
	GetPortalPlayer()->SetJumpedThisFrame( true );
	SetGroundEntity( NULL );

	player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );

	MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );

	float flGroundFactor = 1.0f;
	if (player->m_pSurfaceData)
	{
		flGroundFactor = player->m_pSurfaceData->game.jumpFactor; 
	}

	float flMul = sqrt( 2 * sv_gravity.GetFloat() * 45.f );

	// Acclerate upward
	// If we are ducking...
	Vector vPreJumpVel = mv->m_vecVelocity;
	if ( (  player->m_Local.m_bDucking ) || (  player->GetFlags() & FL_DUCKING ) )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		//mv->m_vecVelocity[2] = flGroundFactor * flMul;  // 2 * gravity * height
		mv->m_vecVelocity -= m_vGravityDirection * DotProduct( mv->m_vecVelocity, m_vGravityDirection );
		mv->m_vecVelocity -= m_vGravityDirection * flGroundFactor * flMul;
	}
	else
	{
		//mv->m_vecVelocity[2] += flGroundFactor * flMul;  // 2 * gravity * height
		mv->m_vecVelocity -= m_vGravityDirection * flGroundFactor * flMul;
	}

	FinishGravity();

	mv->m_outJumpVel += mv->m_vecVelocity - vPreJumpVel;
	mv->m_outStepHeight += 0.15f;

#if !defined( PORTAL2 )
	bool bSetDuckJump = (gpGlobals->maxClients == 1); //most games we only set duck jump if the game is single player
#else
	//const bool bSetDuckJump = true; //in portal 2, do it for both single and multiplayer
	const bool bSetDuckJump = false; // FIX THIS?: This is set to false as a temp fix for camera snapping when ducking in the air due to player maintain the center of the box ( NO DUCKJUMP for now )
#endif

	// Set jump time.
	if ( bSetDuckJump )
	{
		player->m_Local.m_nJumpTimeMsecs = GAMEMOVEMENT_JUMP_TIME;
		player->m_Local.m_bInDuckJump = true;
	}

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wishdir - 
//			accel - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::AirAccelerate( Vector& wishdir, float wishspeed, float accel )
{
	int i;
	float addspeed, accelspeed, currentspeed;
	float wishspd;

	wishspd = wishspeed;

	if (player->pl.deadflag)
		return;

	if (player->m_flWaterJumpTime)
		return;

	// Cap speed
	if (wishspd > 60.0f)
		wishspd = 60.0f;

	// Determine veer amount
	currentspeed = mv->m_vecVelocity.Dot(wishdir);

	// See how much to add
	addspeed = wishspd - currentspeed;

#if defined CLIENT_DLL
	RANDOM_CEG_TEST_SECRET();
#endif

	// If not adding any, done.
	if (addspeed <= 0)
		return;

	CPortal_Player* pPortalPlayer = GetPortalPlayer();
	if( pPortalPlayer == NULL )
		return;

	float fInputScale = pPortalPlayer->GetPortalPlayerLocalData().m_flAirInputScale;

	// Determine acceleration speed after acceleration
	accelspeed = accel * wishspeed * gpGlobals->frametime * player->m_surfaceFriction * fInputScale;

	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust pmove vel.
	for (i=0 ; i<3 ; i++)
	{
		mv->m_vecVelocity[i] += accelspeed * wishdir[i];
		mv->m_outWishVel[i] += accelspeed * wishdir[i];
	}
}

void CPortalGameMovement::TBeamMove( void )
{
	CPortal_Player *pPortalPlayer = GetPortalPlayer();
	if ( !pPortalPlayer )
		return;

	CTrigger_TractorBeam *pTractorBeam = pPortalPlayer->m_PortalLocal.m_hTractorBeam.Get();
	if ( !pTractorBeam )
		return;

	if ( gpGlobals->frametime > 0.0f )
	{
		Vector vLinear;
		AngularImpulse angAngular;
		vLinear.Init();
		angAngular.Init();

		pTractorBeam->CalculateFrameMovement( NULL, pPortalPlayer, gpGlobals->frametime, vLinear, angAngular );
		mv->m_vecVelocity += vLinear * gpGlobals->frametime;
	}

	TryPlayerMove( 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::AirMove( void )
{
	// Determine movement angles
	Vector forward, right, up;
	AngleVectors(mv->m_vecViewAngles, &forward, &right, &up);

	// Copy movement amounts
	float fmove = mv->m_flForwardMove;
	float smove = mv->m_flSideMove;

	{
		// Disregard the player's air movement if they're in a phys controller
		CBasePlayer *pPlayer = GetPortalPlayer();
		if ( pPlayer->HasPhysicsFlag( PFLAG_VPHYSICS_MOTIONCONTROLLER ) )
		{
			fmove = 0;
			smove = 0;
		}
	}

	// Looking mostly straight forward?
	if( forward[2] < 0.5f && forward[2] > -0.5f )
	{
		// Normalize forward vector if they are looking mostly forward
		forward -= DotProduct( forward, m_vGravityDirection ) * m_vGravityDirection;
		VectorNormalize(forward);
	}
	else
	{
		// Do not normalize if they are not.  This prevents the player from screwing up their momentum after
		// exiting floor portals or jumping off sticky ceilings while looking straight up/down.
		forward -= DotProduct( forward, m_vGravityDirection ) * m_vGravityDirection;
	}

	// Zero out components of movement vectors in the direction of gravity
	right -= DotProduct( right, m_vGravityDirection ) * m_vGravityDirection;

	// Normalize remainder of vectors
	VectorNormalize(right);

	Vector targetVel = fmove * forward + smove * right;

	// Zero out velocity in gravity direction
	targetVel -= DotProduct( targetVel, m_vGravityDirection ) * m_vGravityDirection;

	Vector wishdir;
	VectorCopy( targetVel, wishdir );
	Vector vFunnelForce;
	vFunnelForce.Zero();
	//
	// Don't let the player screw their fling because of adjusting into a floor portal
	//
	if ( mv->m_vecVelocity[ 0 ] * mv->m_vecVelocity[ 0 ] + mv->m_vecVelocity[ 1 ] * mv->m_vecVelocity[ 1 ] > MIN_FLING_SPEED * MIN_FLING_SPEED )
	{
		if ( mv->m_vecVelocity[ 0 ] > MIN_FLING_SPEED * 0.5f && wishdir[ 0 ] < 0.0f )
			wishdir[ 0 ] = 0.0f;
		else if ( mv->m_vecVelocity[ 0 ] < -MIN_FLING_SPEED * 0.5f && wishdir[ 0 ] > 0.0f )
			wishdir[ 0 ] = 0.0f;

		if ( mv->m_vecVelocity[ 1 ] > MIN_FLING_SPEED * 0.5f && wishdir[ 1 ] < 0.0f )
			wishdir[ 1 ] = 0.0f;
		else if ( mv->m_vecVelocity[ 1 ] < -MIN_FLING_SPEED * 0.5f && wishdir[ 1 ] > 0.0f )
			wishdir[ 1 ] = 0.0f;
	}

	//
	// Try to autocorrect the player to fall into the middle of the portal
	//
	else if ( sv_player_funnel_into_portals.GetBool() )
	{
		vFunnelForce = PortalFunnel( wishdir );
	}

	// See if we're suppressing air control
	CPortal_Player *pPlayer = GetPortalPlayer();
	if ( pPlayer && pPlayer->IsSuppressingAirControl() )
	{
		wishdir.Zero();
	}

	// Add in funnel force after wishdir potentionally gets nuked.  We still want
	// to funnel, even if the player isnt allowed to move themself
	wishdir += vFunnelForce;

	// Compute speed and direction from velocity
	float targetSpeed = VectorNormalize(wishdir);

	// Clamp to server defined max speed
	if ( targetSpeed != 0 && (targetSpeed > mv->m_flMaxSpeed) )
	{
		VectorScale(targetVel, mv->m_flMaxSpeed/targetSpeed, targetVel);
		targetSpeed = mv->m_flMaxSpeed;
	}

	AirAccelerate( wishdir, targetSpeed, sv_paintairacceleration.GetFloat() );

	// Add in any base velocity to the current velocity.
	VectorAdd(mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	TryPlayerMove( 0, 0 );

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
}

bool CPortalGameMovement::IsInPortalFunnelVolume( const Vector& vPlayerToPortal, const CPortal_Base2D* pPortal, const float flExtentX, const float flExtentY ) const
{
	const Vector& portalNormal = pPortal->m_plane_Origin.normal;
	Vector vPortalRight = pPortal->m_PortalSimulator.GetInternalData().Placement.vRight;
	vPortalRight -= DotProduct( vPortalRight, portalNormal ) * portalNormal;						
	VectorNormalize( vPortalRight );						

	float fTestDist = flExtentX;
	fTestDist *= fTestDist;
	float flDist = ( vPlayerToPortal.Dot( vPortalRight ) * vPortalRight ).LengthSqr();
	// Make sure we're in the 2D portal rectangle
	if ( flDist > fTestDist )
		return false;

	Vector vPortalUp = pPortal->m_PortalSimulator.GetInternalData().Placement.vUp;
	vPortalUp -= DotProduct( vPortalUp, portalNormal ) * portalNormal;
	VectorNormalize( vPortalUp );

	fTestDist = flExtentY;
	fTestDist *= fTestDist;
	flDist = ( vPlayerToPortal.Dot( vPortalUp ) * vPortalUp ).LengthSqr();
	if ( flDist > fTestDist )
		return false;

	return true;
}


bool CPortalGameMovement::PlayerShouldFunnel( const CPortal_Base2D* pPortal, const Vector& vPlayerLook, const Vector& wishdir ) const
{
	// In the air
	if( player->GetGroundEntity() == NULL )
	{
		// We are more liberal about funneling into a ceiling portal - All we care about is if we are going up, in general we aren't going
		// to be hitting these by accident so we can do the funnel without the requirement that we are looking at the portal
		const bool bFunnelUp = mv->m_vecVelocity[ 2 ] > 165.0f;
		if( ( fabsf( wishdir[ 0 ] ) > 64.0f || fabsf( wishdir[ 1 ] ) > 64.0f ) ||
			!( bFunnelUp || ( vPlayerLook.z < -0.7f && mv->m_vecVelocity[ 2 ] < -165.0f ) ) )
		{
			return false;
		}

		// Make sure it's a floor or ceiling portal
		const Vector vPlayerToPortal = pPortal->WorldSpaceCenter() - player->WorldSpaceCenter();
		if( ( bFunnelUp && !pPortal->IsCeilingPortal() ) || 
			( !bFunnelUp && !pPortal->IsFloorPortal() ) )
		{
			return false;
		}

		// Make sure that the portal isn't too far away and we aren't past it.
		const float fPeakRelativeHeight = (mv->m_vecVelocity[2] * mv->m_vecVelocity[2]) / (2.0f * sv_gravity.GetFloat());
		if( ( bFunnelUp && ( (vPlayerToPortal.z > 1024.0f) || (vPlayerToPortal.z <= 0.0f) || (vPlayerToPortal.z > fPeakRelativeHeight) ) ) ||
			( !bFunnelUp && ( (vPlayerToPortal.z < -1024.0f) || (vPlayerToPortal.z >= 0.0f) ) ) )
		{
			return false;
		}

		float flConeScale = RemapValClamped( fabs(vPlayerToPortal.z), 256.f, 1024.f, 1.5, 3.f );
		// Make sure we're directly in line with the portal
		if( !IsInPortalFunnelVolume( vPlayerToPortal,
			pPortal,
			pPortal->m_PortalSimulator.GetInternalData().Placement.fHalfWidth * flConeScale,
			pPortal->m_PortalSimulator.GetInternalData().Placement.fHalfHeight * flConeScale) )
		{
			return false;
		}
	}
	// On the ground and speed funnelling enabled
	else if( speed_funnelling_enabled.GetBool() )
	{
		// The player's max speed isn't larger than walk speed
		if( player->MaxSpeed() <= sv_speed_normal.GetFloat() )
			return false;

		// Not moving toward the portal
		const Vector vPlayerToPortal = pPortal->WorldSpaceCenter() - player->WorldSpaceCenter();
		const Vector& portalNormal = pPortal->m_plane_Origin.normal;
		if( fabs( DotProduct( player->Forward(), portalNormal ) ) < STEEP_SLOPE ||
			DotProduct( player->GetAbsVelocity().Normalized(), portalNormal ) >= -STEEP_SLOPE ||
			DotProduct( wishdir.Normalized(), portalNormal ) >= -STEEP_SLOPE ||
			!IsInPortalFunnelVolume( vPlayerToPortal,
			pPortal,
			pPortal->m_PortalSimulator.GetInternalData().Placement.fHalfWidth * 1.5f,
			pPortal->m_PortalSimulator.GetInternalData().Placement.fHalfHeight * 1.5f ) )
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}


Vector CPortalGameMovement::PortalFunnel( const Vector& wishdir )
{
	Vector vPlayerLook;
	player->EyeVectors( &vPlayerLook );

	const CPortal_Base2D *pFunnelInto = NULL;
	Vector vPlayerToFunnelPortal;
	float fClosestFunnelPortalDistSqr = FLT_MAX;

	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	const int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	for( int i = 0; i != iPortalCount; ++i )
	{
		CPortal_Base2D *pTempPortal = pPortals[i];
		if( pTempPortal->IsActivedAndLinked() )
		{
			const Vector vPlayerToPortal = pTempPortal->WorldSpaceCenter() - player->WorldSpaceCenter();
			const float fDistSqr = vPlayerToPortal.LengthSqr();
			if( PlayerShouldFunnel( pTempPortal, vPlayerLook, wishdir ) &&
				fDistSqr < fClosestFunnelPortalDistSqr )
			{
				fClosestFunnelPortalDistSqr = fDistSqr;
				pFunnelInto = pTempPortal;
				vPlayerToFunnelPortal = vPlayerToPortal;
			}
		}
	}

	Vector vFunnelDir;
	vFunnelDir.Zero();

	if( pFunnelInto )
	{
		// In the air
		if( player->GetGroundEntity() == NULL )
		{
			const float flHeightFromPortal = -vPlayerToFunnelPortal.z - sv_player_funnel_height_adjust.GetFloat();
			// Extra funnel force based on how fast we're going
			float flExtraFunnelForce = RemapValClamped( mv->m_vecVelocity[2], 0.f, 1065.f, 1.f, sv_player_funnel_speed_bonus.GetFloat() );

			// Figure out how long until we hit the portal
			const float flVerticalSpeed = mv->m_vecVelocity.z;
			const float flGravity = player->GetGravity() != 0 ? player->GetGravity() * sv_gravity.GetFloat() : sv_gravity.GetFloat();

			// Solve when we hit
			float flRoot1, flRoot2;
			if( SolveQuadratic( -flGravity, 2.f * flVerticalSpeed, 2.f * flHeightFromPortal, flRoot1, flRoot2 ) )
			{
				float flTimeToPortal = flRoot1 > 0.f ? flRoot1 : flRoot2;
				if( flRoot2 < flRoot1 && flRoot2 >= 0.f && flRoot1 >= 0.f )
					flTimeToPortal = flRoot2;

				AirPortalFunnel( vFunnelDir, vPlayerToFunnelPortal, flExtraFunnelForce, flTimeToPortal );
			}
		}
		// On the ground
		else
		{
			const float flExtraFunnelForce = 1.f + (player->MaxSpeed() / 800.0f) * sv_player_funnel_speed_bonus.GetFloat() - 1.f;
			
			// Compute how long it will take to reach the portal, assuming the player moves at max speed
			const Vector& portalNormal = pFunnelInto->m_plane_Origin.normal;
			const float distanceFromPortal = DotProduct( player->WorldSpaceCenter(), portalNormal ) - pFunnelInto->m_plane_Origin.dist;
			const float flTimeToPortal = distanceFromPortal / player->MaxSpeed();
			
			GroundPortalFunnel( vFunnelDir, vPlayerToFunnelPortal, pFunnelInto->m_plane_Origin.normal, flExtraFunnelForce, flTimeToPortal );
		}
	}//if funnelling

	return vFunnelDir;
}

void CPortalGameMovement::AirPortalFunnel( Vector& wishdir,
										   const Vector& vPlayerToFunnelPortal,
										   float flExtraFunnelForce,
										   float flTimeToPortal )
{
	float flExponent = RemapValClamped( fabs(vPlayerToFunnelPortal.z), 128.f, 1024.f, 0.01f, 0.15 );
	const float flDecay = ExponentialDecay( flExponent, gpGlobals->frametime );

	for( int i = 0; i < 2; ++i )
	{
		// Find out if we'll make it to our destination in time
		bool bMakeItInTime = false;
		if( mv->m_vecVelocity[i] )
		{
			const float flTimeToCenter = vPlayerToFunnelPortal[i] / mv->m_vecVelocity[i];
			bMakeItInTime = flTimeToCenter < flTimeToPortal;
		}

		// If we're not going to make it, then accellerate
		if( !bMakeItInTime )
		{
			// Funnel toward the portal
			const float flFunnel = vPlayerToFunnelPortal[i] * flExtraFunnelForce * PORTAL_FUNNEL_AMOUNT - mv->m_vecVelocity[i];
			wishdir[i] += flFunnel;
		}
		else
		{
			if( fabs( mv->m_vecVelocity[i] ) > sv_player_funnel_snap_threshold.GetFloat() )
			{
				// Decay speed in XY plane if we're moving too quick
				mv->m_vecVelocity[i] = mv->m_vecVelocity[i] * flDecay;
			}
			else
			{
				// Just stop if below threshold
				mv->m_vecVelocity[i] = 0.f;	
			}
		}

		if ( portal_funnel_debug.GetBool() )
		{
			char* idx[] = { "x", "y" };
			Msg( "%s: Make in time? %s velocity %f wish %f\n", idx[i], 
																(bMakeItInTime) ? ("yes") : ("no"), 
																mv->m_vecVelocity[i],
																wishdir[i] );
		}
	}//for

	if ( portal_funnel_debug.GetBool() )
	{
		Msg( "Extra force: %f Time till portal: %f\n", flExtraFunnelForce, flTimeToPortal );
	}

}


void CPortalGameMovement::GroundPortalFunnel( Vector& wishdir,
											  const Vector& vPlayerToFunnelPortal,
											  const Vector& vPortalNormal,
											  float flExtraFunnelForce,
											  float flTimeToPortal )
{
	// Compute the velocity tangent to the portal plane in the horizontal direction
	const Vector tangentialVelocity = mv->m_vecVelocity - DotProduct( mv->m_vecVelocity, vPortalNormal ) * vPortalNormal;
	Vector horizontalVelocity = tangentialVelocity;
	horizontalVelocity -= DotProduct( horizontalVelocity, vPortalNormal ) * vPortalNormal;
	horizontalVelocity.z = 0.0f;
	
	// Compute how long it'll take for the player to be centered at the current horizontal speed
	Vector vPlayerToPortalHorizontal = vPlayerToFunnelPortal;
	vPlayerToPortalHorizontal -= DotProduct( vPlayerToPortalHorizontal, vPortalNormal ) * vPortalNormal;
	vPlayerToPortalHorizontal.z = 0.0f;
	Vector horizontalVelocityDir = horizontalVelocity;
	Vector horizontalPlayerToPortalDir = vPlayerToPortalHorizontal;
	const float horizontalSpeed = horizontalVelocityDir.NormalizeInPlace();
	const float horizontalDistance = horizontalPlayerToPortalDir.NormalizeInPlace();
	const float flTimeToCenter = horizontalDistance / horizontalSpeed;

	// If the player won't be centered by the time she reaches the portal
	if( flTimeToCenter > flTimeToPortal )
	{
		// Figure out if velocity and funnel impulse are opposite each other
		const float flVelocityCrossNormal = horizontalVelocityDir.x * vPortalNormal.y - horizontalVelocityDir.y * vPortalNormal.x; 
		const float flToCenterCrossNormal = horizontalPlayerToPortalDir.x * vPortalNormal.y - horizontalPlayerToPortalDir.y * vPortalNormal.x;
		const float flSign = Sign( flVelocityCrossNormal * flToCenterCrossNormal );

		// Funnel the player toward the center
		const float flFunnel = fpmax( horizontalDistance * flExtraFunnelForce - flSign * horizontalSpeed, 0.0f );
		Vector vFunnelVector = flFunnel * horizontalPlayerToPortalDir;
		vFunnelVector -= DotProduct( vPortalNormal, vFunnelVector ) * vPortalNormal;
		wishdir += vFunnelVector;
		//NDebugOverlay::HorzArrow( player->WorldSpaceCenter(), player->WorldSpaceCenter() + flFunnel * horizontalPlayerToPortalDir, 5.0f, 255, 0, 0, 128, true, 5.0f );
		//DevMsg( "Funnel %s: %f\n", DotProduct( player->Left(), horizontalPlayerToPortalDir ) > 0 ? "Left" : "Right", flFunnel );
		//DevMsg( "Horizontal Distance: %f\n", horizontalDistance );
		//DevMsg( "Horizontal Speed: %f\n", horizontalSpeed );
		//DevMsg( "Normal Funneling: %f\n", DotProduct( vPortalNormal, vFunnelVector) );
	}
}


CEG_NOINLINE void CPortalGameMovement::PlayerRoughLandingEffects( float fvol )
{
	if ( fvol > 0.0 )
	{
		//
		// Play landing sound right away.
		player->m_flStepSoundTime = 400;

		// Play step sound for current texture.
		player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, MAX( fvol, 0.1f ), true );

//		//
//		// Knock the screen around a little bit, temporary effect.
//		//
//		player->m_Local.m_vecPunchAngle.Set( ROLL, player->m_Local.m_flFallVelocity * 0.013 );
//
//		if ( player->m_Local.m_vecPunchAngle[PITCH] > 8 )
//		{
//			player->m_Local.m_vecPunchAngle.Set( PITCH, 8 );
//		}
//
//#if !defined( CLIENT_DLL )
//		player->RumbleEffect( ( fvol > 0.85f ) ? ( RUMBLE_FALL_LONG ) : ( RUMBLE_FALL_SHORT ), 0, RUMBLE_FLAGS_NONE );
//#endif
	}

	if ( fvol >= 1.0 )
	{
		// Play the future shoes sound
		CRecipientFilter filter;
		filter.UsePredictionRules();
		filter.AddRecipientsByPAS( player->GetAbsOrigin() );

		CSoundParameters params;
		if ( CBaseEntity::GetParametersForSound( "PortalPlayer.FallRecover", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_nPitch = 125.0f - player->m_Local.m_flFallVelocity * 0.03f;					// lower pitch the harder they land
			ep.m_flVolume = MIN( player->m_Local.m_flFallVelocity * 0.00075f - 0.38, 1.0f );	// louder the harder they land

#if defined GAME_DLL
			CEG_PROTECT_MEMBER_FUNCTION( CPortalGameMovement_PlayerRoughLandingEffects );
#endif

			CBaseEntity::EmitSound( filter, player->entindex(), ep );
		}
	}
}

void CPortalGameMovement::PlayerWallImpactEffects( float fvol, float normalVelocity )
{
	if ( fvol > 0.0 )
	{
		//
		// Play landing sound right away.
		player->m_flStepSoundTime = 400;

		// Play step sound for current texture.
		player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, MAX( fvol, 0.1f ), true );
	}

	if ( fvol >= 1.0 )
	{
		// Play the impact sound
		CRecipientFilter filter;
		filter.UsePredictionRules();
		filter.AddRecipientsByPAS( player->GetAbsOrigin() );

		CSoundParameters params;
		if ( CBaseEntity::GetParametersForSound( "JumpLand.HighVelocityImpact", params, NULL ) )
		{
			EmitSound_t ep( params );
			ep.m_nPitch = 125.0f - 2.0f * normalVelocity * 0.03f;					// lower pitch the harder they land
			ep.m_flVolume = MIN( 2.0f * (normalVelocity * 0.00075f - 0.38), 1.0f );	// louder the harder they land

			CBaseEntity::EmitSound( filter, player->entindex(), ep );
		}
	}
}

void CPortalGameMovement::PlayerCeilingImpactEffects( float fvol )
{
	//if ( fvol > 0.0 )
	//{
	//	//
	//	// Play landing sound right away.
	//	player->m_flStepSoundTime = 400;

	//	// Play step sound for current texture.
	//	player->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, MAX( fvol, 0.1f ), true );
	//}

	if ( fvol >= 1.0 )
	{
		// Play the impact sound
		CRecipientFilter filter;
		filter.UsePredictionRules();
		filter.AddRecipientsByPAS( player->GetAbsOrigin() );

		CSoundParameters params;
		if ( CBaseEntity::GetParametersForSound( "JumpLand.HighVelocityImpactCeiling", params, NULL ) )
		{
			const float fallVelocity = fabs( player->m_Local.m_flFallVelocity );
			EmitSound_t ep( params );
			ep.m_nPitch = 125.0f - 2.0f * fallVelocity * 0.03f;					// lower pitch the harder they land
			ep.m_flVolume = MIN( 4.0f * (fallVelocity * 0.00075f - 0.38), 1.0f );	// louder the harder they land

#if defined CLIENT_DLL
			RANDOM_CEG_TEST_SECRET_PERIOD( 3, 7 );
#endif

			CBaseEntity::EmitSound( filter, player->entindex(), ep );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &input - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::CategorizePosition( void )
{
	//DiffPrint( "Categorize() %f %f %f  %f %f %f", XYZ( mv->GetAbsOrigin() ), XYZ( mv->m_vecVelocity ) );

	// TODO: If we decide to turn friction down while on speed paint, this needs to change
	// Reset this each time we-recategorize, otherwise we have bogus friction when we jump into water and plunge downward really quickly
	player->m_surfaceFriction = 1.0f;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	CheckWater();

	// observers don't have a ground entity
	if ( player->IsObserver() )
		return;

	const float GROUND_OFFSET = 2.0f;
	const Vector& stickNormal = GetPortalPlayer()->GetPortalPlayerLocalData().m_StickNormal;
	Vector point = mv->GetAbsOrigin() + GROUND_OFFSET * m_vGravityDirection;
	Vector bumpOrigin = mv->GetAbsOrigin();

	// Shooting up really fast.  Definitely not on ground.
	// On ladder moving up, so not on ground either
	// NOTE: 145 is a jump.
	const float NON_JUMP_VELOCITY = 140.0f;

	CPortal_Player* pPortalPlayer = GetPortalPlayer();
	const float normalVel = DotProduct( mv->m_vecVelocity, stickNormal );
	const bool bMovingUp = normalVel > 0.0f;
	bool bMovingUpRapidly = normalVel > NON_JUMP_VELOCITY && ((player->GetGroundEntity() == NULL) || IsInactivePower( pPortalPlayer->GetPaintPower( SPEED_POWER )));
	float flGroundEntityNormalVel = 0.0f;
	if ( bMovingUpRapidly )
	{
		// Tracker 73219, 75878:  ywb 8/2/07
		// After save/restore (and maybe at other times), we can get a case where we were saved on a lift and 
		//  after restore we'll have a high local velocity due to the lift making our abs velocity appear high.  
		// We need to account for standing on a moving ground object in that case in order to determine if we really 
		//  are moving away from the object we are standing on at too rapid a speed.  Note that CheckJump already sets
		//  ground entity to NULL, so this wouldn't have any effect unless we are moving up rapidly not from the jump button.
		CBaseEntity *ground = player->GetGroundEntity();
		if ( ground )
		{
			flGroundEntityNormalVel = DotProduct( ground->GetAbsVelocity(), stickNormal );
			bMovingUpRapidly = ( normalVel - flGroundEntityNormalVel ) > NON_JUMP_VELOCITY;
		}
	}

	// NOTE YWB 7/5/07:  Since we're already doing a traceline here, we'll subsume the StayOnGround (stair debouncing) check into the main traceline we do here to see what we're standing on
	const bool bUnderwater = ( player->GetWaterLevel() >= WL_Eyes );
	bool bMoveToEndPos = false;
	if ( player->GetMoveType() == MOVETYPE_WALK && 
		player->GetGroundEntity() != NULL && !bUnderwater )
	{
		// if walking and still think we're on ground, we'll extend trace down by stepsize so we don't bounce down slopes
		// We scale by the ratio of max speed to normal speed to account for speed paint.
		bMoveToEndPos = true;
		point += ( player->MaxSpeed() / sv_speed_normal.GetFloat() ) * player->m_Local.m_flStepSize * m_vGravityDirection;
	}

	// Was on ground, but now suddenly am not
	CTrace_PlayerAABB_vs_Portals pm;

	if ( bMovingUpRapidly || 
		( bMovingUp && player->GetMoveType() == MOVETYPE_LADDER ) )   
	{
		SetGroundEntity( NULL );
		bMoveToEndPos = false;
	}
	else
	{
		// Try to move down.
		TracePlayerBBox( bumpOrigin, point, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

		const Vector vPrevGroundNormal = pPortalPlayer->GetPrevGroundNormal();

		float fDot = DotProduct( vPrevGroundNormal, pm.plane.normal );
		float fSpeed = mv->m_vecVelocity.Length();
		bool bRampLaunch = false;

		// Fast enough to launch and surface slopes have a sharp enough change?
		if( pPortalPlayer->GetGroundEntity() &&		// We're on the ground this frame
			fSpeed > sv_maxspeed.GetFloat() &&	// Our speed is greater than the normal (ie. we're on speed paint)
			( fDot < 1.f ||	!pm.DidHit() )	)	// And the trace did not hit or there was a significant change in surface normals
		{

			// Find out if the slope went up or down
			Vector vVelCrossUp = CrossProduct( mv->m_vecVelocity.Normalized(), -m_vGravityDirection );
			Vector vNewNormCrossOldNorm = CrossProduct( pm.plane.normal, vPrevGroundNormal );
			float fCrossDot = DotProduct( vVelCrossUp, vNewNormCrossOldNorm );
			// If it was a downward change then launch off of it
			if( fCrossDot > EQUAL_EPSILON || ( vPrevGroundNormal.Length2DSqr() && !pm.DidHit() ) )
			{
				bRampLaunch = true;

#if defined CLIENT_DLL
				STEAMWORKS_TESTSECRET();
#endif

				// Compute normalized forward direction in tangent plane of the ramp
				const Vector vWishDirection = mv->m_vecVelocity;
				const Vector vTangentRight = CrossProduct( vWishDirection, vPrevGroundNormal );
				const Vector vNormTangentForward = CrossProduct( vPrevGroundNormal, vTangentRight ).Normalized();

				mv->m_vecVelocity = vNormTangentForward * mv->m_vecVelocity.Length();
			}
		}

		// Was on ground, but now suddenly am not.  If we hit a steep plane, we are not on ground
		const float flStandableAngle = CRITICAL_SLOPE;
		const float traceNormalAngle = DotProduct( pm.plane.normal, stickNormal );
		if ( !pm.m_pEnt || (( traceNormalAngle < flStandableAngle ) && !pm.HitPortalRamp(stickNormal)) || bRampLaunch )
		{
			//DiffPrint( "Categorize() Lost ground ent %i %i", player->GetGroundEntity() ? player->GetGroundEntity()->entindex() : -1, pm.m_pEnt ? pm.m_pEnt->entindex() : -1  );

			// Test four sub-boxes, to see if any of them would have found shallower slope we could actually stand on
			ITraceFilter *pFilter = LockTraceFilter( COLLISION_GROUP_PLAYER_MOVEMENT );
			PortalTracePlayerBBoxForGround( bumpOrigin, point, GetPlayerMins(), GetPlayerMaxs(),
				mv->m_nPlayerHandle.Get(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT,
				pm, -m_vGravityDirection );
			UnlockTraceFilter( pFilter );

			if ( !pm.m_pEnt || (( traceNormalAngle < flStandableAngle ) && !pm.HitPortalRamp(stickNormal)) || bRampLaunch )
			{
				//DiffPrint( "Categorize() failed to find new one" );
				SetGroundEntity( NULL );
				// probably want to add a check for a +z velocity too!
				if ( ( normalVel > 0.0f ) && 
					( player->GetMoveType() != MOVETYPE_NOCLIP ) )
				{
					player->m_surfaceFriction = 0.25f;
				}
				bMoveToEndPos = false;
			}
			else
			{
				//DiffPrint( "Categorize() found new one" );
				SetGroundEntity( &pm );
			}
		}
		else
		{
			SetGroundEntity( &pm );  // Otherwise, point to index of ent under us.
		}

#ifndef CLIENT_DLL

		//Adrian: vehicle code handles for us.
		if ( player->IsInAVehicle() == false )
		{
			// If our gamematerial has changed, tell any player surface triggers that are watching
			IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
			surfacedata_t *pSurfaceProp = physprops->GetSurfaceData( pm.surface.surfaceProps );
			char cCurrGameMaterial = pSurfaceProp->game.material;
			if ( !player->GetGroundEntity() )
			{
				cCurrGameMaterial = 0;
			}

			// Changed?
			if ( player->m_chPreviousTextureType != cCurrGameMaterial )
			{
				CEnvPlayerSurfaceTrigger::SetPlayerSurface( player, cCurrGameMaterial );
			}

			player->m_chPreviousTextureType = cCurrGameMaterial;
		}
#endif
	}

	// YWB:  This logic block essentially lifted from StayOnGround implementation
	if ( bMoveToEndPos &&
		!pm.startsolid &&				// not sure we need this check as fraction would == 0.0f?
		pm.fraction > 0.0f &&			// must go somewhere
		pm.fraction < 1.0f &&			// must hit something
		gpGlobals->frametime != 0.f ) 		// can't divide by zero
	{
		mv->SetAbsOrigin( pm.endpos );
		const float flDelta = DotProduct( mv->GetAbsOrigin() - pm.endpos, m_vGravityDirection );
		// Set the implicit vertical speed keeping the player on the surface
		// Ignore this one if it's on an angled surface, the player is moving, and we get a zero.
		// The values cycle back to zero occasionally while moving on sloped surfaces, which doesn't accurately reflect this implicit speed.
		if( (gpGlobals->frametime != 0.0f) && (flDelta != 0.0f || AlmostEqual( pm.plane.normal.z, 1.0f ) || ( mv->m_flSideMove == 0.0f && mv->m_flForwardMove == 0.0f )) )
			GetPortalPlayer()->SetImplicitVerticalStepSpeed( flDelta / gpGlobals->frametime );		
	}

	// Save the normal of the surface if we hit something
	if( pPortalPlayer->GetGroundEntity() != NULL && pm.DidHit() )
	{
		pPortalPlayer->SetPrevGroundNormal( pm.plane.normal );
	}
}


int CPortalGameMovement::CheckStuck( void )
{
	if( BaseClass::CheckStuck() )
	{
		CPortal_Player *pPortalPlayer = GetPortalPlayer();

#if !defined( CLIENT_DLL ) && !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
		if( pPortalPlayer->IsAlive() )
			g_PortalGameStats.Event_PlayerStuck( pPortalPlayer );
#endif

		//try to fix it, then recheck
		Vector vIndecisive;
		if( pPortalPlayer->m_hPortalEnvironment )
		{
			pPortalPlayer->m_hPortalEnvironment->GetVectors( &vIndecisive, NULL, NULL );
		}
		else
		{
			vIndecisive.Init( 0.0f, 0.0f, 1.0f );
		}
		Vector ptOldOrigin = pPortalPlayer->GetAbsOrigin();

		if( pPortalPlayer->m_hPortalEnvironment )
		{
			if( !FindClosestPassableSpace( pPortalPlayer, vIndecisive, MASK_PLAYERSOLID ) )
			{
#ifndef CLIENT_DLL
				DevMsg( "Hurting the player for FindClosestPassableSpaceFailure!" );

				CTakeDamageInfo info( pPortalPlayer, pPortalPlayer, vec3_origin, vec3_origin, 1e10, DMG_CRUSH );
				pPortalPlayer->OnTakeDamage( info );
#endif
			}

			//make sure we didn't get put behind the portal >_<
			Vector ptCurrentOrigin = pPortalPlayer->GetAbsOrigin();
			if( vIndecisive.Dot( ptCurrentOrigin - ptOldOrigin ) < 0.0f )
			{
				pPortalPlayer->SetAbsOrigin( ptOldOrigin + (vIndecisive * 5.0f) ); //this is an anti-bug hack, since this would have probably popped them out of the world, we're just going to move them forward a few units
			}
		}

		mv->SetAbsOrigin( pPortalPlayer->GetAbsOrigin() );
		return BaseClass::CheckStuck();
	}
	else
	{
		return 0;
	}
}

void CPortalGameMovement::SetGroundEntity( trace_t *pm )
{
	CBaseEntity *newGround = pm ? pm->m_pEnt : NULL;

	CBaseEntity *oldGround = player->GetGroundEntity();
	Vector vecBaseVelocity = player->GetBaseVelocity();

	if ( !oldGround && newGround )
	{
		// Subtract ground velocity at instant we hit ground jumping
		vecBaseVelocity -= newGround->GetAbsVelocity(); 

		Vector vecBaseDotGrav = m_vGravityDirection * DotProduct( m_vGravityDirection, vecBaseVelocity );
		Vector vecAbsDotGrav = m_vGravityDirection * DotProduct( m_vGravityDirection, newGround->GetAbsVelocity() );
		vecBaseVelocity += vecAbsDotGrav - vecBaseDotGrav;

#ifdef GAME_DLL

		CPortal_Player* pPlayer = ToPortalPlayer( player );
		if( pPlayer )
		{
			pPlayer->ResetBounceCount();
			pPlayer->ResetAirTauntCount();
			pPlayer->OnPlayerLanded();
		}

		IGameEvent* pEvent = gameeventmanager->CreateEvent("player_landed");
		if( pEvent )
		{
			pEvent->SetInt("userid", player->GetUserID() );
			gameeventmanager->FireEvent( pEvent );
		}
#endif
	}
	else if ( oldGround && !newGround )
	{
		// Add in ground velocity at instant we started jumping
		vecBaseVelocity += oldGround->GetAbsVelocity();

		Vector vecBaseDotGrav = m_vGravityDirection * DotProduct( m_vGravityDirection, vecBaseVelocity );
		Vector vecAbsDotGrav = m_vGravityDirection * DotProduct( m_vGravityDirection, oldGround->GetAbsVelocity() );
		vecBaseVelocity += vecAbsDotGrav - vecBaseDotGrav;
	}

	player->SetBaseVelocity( vecBaseVelocity );
	player->SetGroundEntity( newGround );

	// If we are on something...

	if ( newGround )
	{
		CategorizeGroundSurface( *pm );

		// Then we are not in water jump sequence
		player->m_flWaterJumpTime = 0;

		// Standing on an entity other than the world, so signal that we are touching something.
		if ( !pm->DidHitWorld() )
		{
			MoveHelper()->AddToTouched( *pm, mv->m_vecVelocity );
		}

		if( player->GetMoveType() != MOVETYPE_NOCLIP )
		{
			mv->m_vecVelocity -= m_vGravityDirection * DotProduct( mv->m_vecVelocity, m_vGravityDirection );
		}
	}
}

//#define TRACE_DEBUG_ENABLED //uncomment for trace debugging goodness
#if defined( TRACE_DEBUG_ENABLED )
bool g_bTraceDebugging = false;
struct StackPair_t
{
	void *pStack[32];
	int iCount;
	CPortal_Base2D *pPortal;
	Vector vStart;
	Vector vEnd;
	Ray_t ray;
	trace_t realTrace;
	trace_t portalTrace1;
	trace_t portalTrace2;
	trace_t remoteTubeTrace;
	trace_t angledWallTrace;
	trace_t finalTrace;

	bool bCopiedRemoteTubeTrace;
	bool bCopiedAngledWallTrace;
	bool bCopiedPortal2Trace;
};

struct StackPairWithBuf_t : public StackPair_t
{
	char szStackTranslated[2048];
};
StackPairWithBuf_t g_TraceRealigned[32]; //for debugging
#define TRACE_DEBUG_ONLY(x) x
#else
#define TRACE_DEBUG_ONLY(x)
#endif

/*bool g_bSpewTraceStuck = false;
class CSpewTraceStuckContext
{
public:
	CSpewTraceStuckContext( void ) { g_bSpewTraceStuck = true; }
	~CSpewTraceStuckContext( void ) { g_bSpewTraceStuck = false; }
};*/

class CTraceTypeWrapper : public ITraceFilter
{
public:
	CTraceTypeWrapper( TraceType_t type, ITraceFilter *pBaseFilter )
		: m_Type( type ), m_pBase( pBaseFilter )
	{ }

	virtual bool ShouldHitEntity( IHandleEntity *pEntity, int contentsMask )
	{
		return m_pBase->ShouldHitEntity( pEntity, contentsMask );
	}

	virtual TraceType_t	GetTraceType() const
	{
		return m_Type;
	}

private:
	TraceType_t m_Type;
	ITraceFilter *m_pBase;
};

Vector CalculateExtentShift( const Vector &vLocalExtents, const Vector &vLocalNormal, const Vector &vRemoteExtents, const Vector &vRemoteNormal )
{
	//for visualization of what fClosestExtentDiff is for.....
	//if walking into the floor (standing hull) and coming out a wall (duck hull but doesn't matter). fClosestExtentsDiff will be negative.
	//when this negative value is applied to the linked portal plane normal, it shifts it relatively backwards to it, but closer to us in local space
	//the net effect is that the local ray's foot is exactly on the portal plane, and the exit ray's side is exactly on the portal plane.
	//without the modifier, the exit ray would be roughly 20 units behind the exit plane in this hypothetical
#if 1
	float fClosestExtentDiff = 0.0f;
	for( int i = 0; i != 3; ++i )
	{
		fClosestExtentDiff -= fabs( vLocalNormal[i] * vLocalExtents[i] );
		fClosestExtentDiff += fabs( vRemoteNormal[i] * vRemoteExtents[i] );
	}
#else
	float fMaxLocalExtentDiff = fabs( vLocalNormal[0] * vLocalExtents[0] );
	int iMaxLocalExtent = 0;
	float fMaxRemoteExtentDiff = fabs( vRemoteNormal[0] * vRemoteExtents[0] );
	int iMaxRemoteExtent = 0;
	for( int i = 1; i != 3; ++i )
	{
		float fLocalExtentDiff = fabs( vLocalNormal[i] * vLocalNormal[i] );
		if( fLocalExtentDiff > fMaxLocalExtentDiff )
		{
			fMaxLocalExtentDiff = fLocalExtentDiff;
			iMaxLocalExtent = i;
		}

		float fRemoteExtentDiff = fabs( vRemoteNormal[i] * vRemoteExtents[i] );
		if( fRemoteExtentDiff > fMaxRemoteExtentDiff )
		{
			fMaxRemoteExtentDiff = fRemoteExtentDiff;
			iMaxRemoteExtent = i;
		}
	}
#if 0
	float fClosestExtentDiff = fMaxRemoteExtentDiff - fMaxLocalExtentDiff;
#else
	float fClosestExtentDiff = vRemoteNormal[iMaxRemoteExtent] - vLocalExtents[iMaxLocalExtent];
#endif
#endif

	return (vRemoteNormal * fClosestExtentDiff);
}

ConVar sv_portal_new_player_trace_vs_remote_ents( "sv_portal_new_player_trace_vs_remote_ents", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

#define DEBUG_PORTAL_TRACE_BOXES //uncomment to draw collision debug info near portals

#if defined( DEBUG_PORTAL_TRACE_BOXES )
ConVar sv_portal_new_trace_debugboxes( "sv_portal_new_trace_debugboxes", "0", FCVAR_REPLICATED );
#endif

void TracePortalPlayerAABB( CPortal_Player *pPortalPlayer, CPortal_Base2D *pPortal, const Ray_t &ray_local, const Ray_t &ray_remote, const Vector &vRemoteShift, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals &pm, bool bSkipRemoteTubeCheck = false )
{
#ifdef CLIENT_DLL
	CTraceFilterSimple traceFilter( pPortalPlayer, collisionGroup );
#else
	CTraceFilterSimple baseFilter( pPortalPlayer, collisionGroup );
	CTraceFilterTranslateClones traceFilter( &baseFilter );

	//verify agreement on whether the player is in a portal simulator or not
	Assert( (pPortalPlayer->m_hPortalEnvironment.Get() != NULL) || (CPortalSimulator::GetSimulatorThatOwnsEntity( pPortalPlayer ) == NULL) );
#endif

	pm.m_bContactedPortalTransitionRamp = false;

	if( sv_portal_new_player_trace.GetBool() )
	{
#if defined( TRACE_DEBUG_ENABLED )
		static StackPair_t s_LastTrace[32];
		static int s_iLastTrace = 0;
		StackPair_t &traceDebugEntry = s_LastTrace[s_iLastTrace];
		if( g_bTraceDebugging )
		{
			for( int i = 0; i < 32; ++i )
			{
				int iOldestIterate = (s_iLastTrace + i) % 32; //s_iLastTrace starts pointing at the oldest entry
				memcpy( &g_TraceRealigned[31 - i], &s_LastTrace[iOldestIterate], sizeof( StackPair_t ) );			
			}

			for( int i = 0; i < 32; ++i )
			{
				TranslateStackInfo( g_TraceRealigned[i].pStack, g_TraceRealigned[i].iCount, g_TraceRealigned[i].szStackTranslated, sizeof( g_TraceRealigned[i].szStackTranslated ), "\n" );
			}
			_asm nop;
		}
		else
		{
			traceDebugEntry.vStart = ray_local.m_Start + ray_local.m_StartOffset;
			traceDebugEntry.vEnd = ray_local.m_Start + ray_local.m_Delta + ray_local.m_StartOffset;
			traceDebugEntry.pPortal = pPortalPlayer->m_hPortalEnvironment;
			traceDebugEntry.iCount = GetCallStack_Fast( traceDebugEntry.pStack, 32, 0 );
			traceDebugEntry.ray = ray_local;


			UTIL_ClearTrace( traceDebugEntry.realTrace );
			UTIL_ClearTrace( traceDebugEntry.portalTrace1 );
			UTIL_ClearTrace( traceDebugEntry.portalTrace2 );
			UTIL_ClearTrace( traceDebugEntry.remoteTubeTrace );
			UTIL_ClearTrace( traceDebugEntry.angledWallTrace );
			UTIL_ClearTrace( traceDebugEntry.finalTrace );

			traceDebugEntry.bCopiedRemoteTubeTrace = false;
			traceDebugEntry.bCopiedAngledWallTrace = false;
			traceDebugEntry.bCopiedPortal2Trace = false;
		}		
		++s_iLastTrace;
		s_iLastTrace = s_iLastTrace % 32;
#endif

		trace_t RealTrace;
		UTIL_ClearTrace( RealTrace );
		enginetrace->TraceRay( ray_local, fMask, &traceFilter, &RealTrace );
		TRACE_DEBUG_ONLY( traceDebugEntry.realTrace = RealTrace );


		/*RayInPortalHoleResult_t rayPortalIntersection = RIPHR_NOT_TOUCHING_HOLE;
		if( pPortal && pPortal->m_PortalSimulator.IsReadyToSimulate() )
		{
			rayPortalIntersection = pPortal->m_PortalSimulator.IsRayInPortalHole( ray_local );
		}

		if( rayPortalIntersection != RIPHR_NOT_TOUCHING_HOLE )*/
		if( pPortal && pPortal->IsActivedAndLinked() && pPortal->m_PortalSimulator.IsReadyToSimulate() && (RealTrace.startsolid || (ray_local.m_IsSwept && (RealTrace.fraction < 1.0f))) )
		{
#if defined( DEBUG_PORTAL_TRACE_BOXES )
			color24 debugBoxColor = { 0, 0, 0 };
#endif
			trace_t PortalTrace;
			UTIL_ClearTrace( PortalTrace );
			UTIL_Portal_TraceRay( pPortal, ray_local, fMask, &traceFilter, &PortalTrace, true );
			TRACE_DEBUG_ONLY( traceDebugEntry.portalTrace1 = PortalTrace );

			//Assert( RealTrace.startsolid || !PortalTrace.startsolid || (RealTrace.plane.dist == PortalTrace.plane.dist) );

			if( RealTrace.startsolid || (!PortalTrace.startsolid && (ray_local.m_IsSwept && (PortalTrace.fraction >= RealTrace.fraction))) ) //portal trace provides a better option
			{
#if defined( DEBUG_PORTAL_TRACE_BOXES )
				if( !PortalTrace.startsolid )
				{
					debugBoxColor.g = 255;
				}
#endif

				if( RealTrace.m_pEnt && RealTrace.m_pEnt->IsWorld() )
				{
					RealTrace.m_pEnt = pPortal->m_PortalSimulator.GetInternalData().Simulation.hCollisionEntity;
				}

				const PS_InternalData_t &portalSimulator = pPortal->m_PortalSimulator.GetInternalData();

				CPortal_Base2D *pLinkedPortal = pPortal->m_hLinkedPortal;
				//const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

				bool bContactedTransitionRamp = false;
				trace_t PortalTrace2;
				UTIL_ClearTrace( PortalTrace2 );

#define REMOTE_TRACE_WALL_IS_ONLY_TUBE
				
				//trace as an AABB against only the world in the remote space. We don't trace entities because of the projected floor to wall dillemma where we can ledge walk in the middle of the portal
				CTraceTypeWrapper worldOnlyTraceFilterWrapper( TRACE_WORLD_ONLY, &traceFilter );
				ITraceFilter *pRemoteTraceFilter;
				if( sv_portal_new_player_trace_vs_remote_ents.GetBool() ) //weird, couldn't use ternary operator for this assignment
				{
					pRemoteTraceFilter = &traceFilter;
				}
				else
				{
					pRemoteTraceFilter = &worldOnlyTraceFilterWrapper;
				}
#if defined( REMOTE_TRACE_WALL_IS_ONLY_TUBE )
				UTIL_Portal_TraceRay( pLinkedPortal, ray_remote, fMask, pRemoteTraceFilter, &PortalTrace2, false );	
#else
				UTIL_Portal_TraceRay( pLinkedPortal, ray_remote, fMask, pRemoteTraceFilter, &PortalTrace2, true );	
#endif
				TRACE_DEBUG_ONLY( traceDebugEntry.portalTrace2 = PortalTrace2 );

				//slightly angled portal transitions can present their remote-space collision as an extremely steep slope.
				//Step code fails on that kind of step because it's both non-standable and just barely too far forward to step onto
				//this next bit is to detect those cases and present the slope (no matter how steep) as standable
				if( portalSimulator.Placement.pAABBAngleTransformCollideable )
				{
					trace_t AngleTrace;
					UTIL_ClearTrace( AngleTrace );
					physcollision->TraceBox( ray_remote, portalSimulator.Placement.pAABBAngleTransformCollideable, portalSimulator.Placement.ptaap_ThisToLinked.ptOriginTransform, portalSimulator.Placement.ptaap_ThisToLinked.qAngleTransform, &AngleTrace );
					TRACE_DEBUG_ONLY( traceDebugEntry.angledWallTrace = AngleTrace );

					//allow super steep slope climbs if the player is contacting the transition ramp
					if( AngleTrace.startsolid || (ray_remote.m_IsSwept && (AngleTrace.fraction <= PortalTrace.fraction) && (AngleTrace.fraction < 1.0f)) ) //note, only has to go as far as local trace to work
					{
						bContactedTransitionRamp = true;
#if defined( DEBUG_PORTAL_TRACE_BOXES )
						debugBoxColor.r = 255;
#endif
					}

#if 0 //on second thought, maybe you shouldn't actually walk on this magic ramp
					if( ray_remote.m_IsSwept && (AngleTrace.fraction < PortalTrace2.fraction) )
					{
						PortalTrace2 = AngleTrace;
						bContactedTransitionRamp = true;
						TRACE_DEBUG_ONLY( traceDebugEntry.bCopiedAngledWallTrace = true );
					}
#endif
				}

#if defined( REMOTE_TRACE_WALL_IS_ONLY_TUBE )
				//This trace is SUPER IMPORTANT. While the AABB will always be consistent. It can be relatively different when portalling.
				//We need to test that the player will fit through the portal in both configurations.
				//To do that, we'll create a super AABB which will be a local-space maximal AABB of the local AABB and the transformed OBB
				if( !bSkipRemoteTubeCheck && portalSimulator.Simulation.Static.Wall.Local.Tube.pCollideable )
				{
					trace_t TubeTrace;
					UTIL_ClearTrace( TubeTrace );
					physcollision->TraceBox( ray_remote, portalSimulator.Simulation.Static.Wall.Local.Tube.pCollideable, portalSimulator.Placement.ptaap_ThisToLinked.ptOriginTransform, portalSimulator.Placement.ptaap_ThisToLinked.qAngleTransform, &TubeTrace );			
					TRACE_DEBUG_ONLY( traceDebugEntry.remoteTubeTrace = TubeTrace );

					if( TubeTrace.startsolid || (ray_remote.m_IsSwept && (TubeTrace.fraction < PortalTrace2.fraction)) )
					{
						PortalTrace2 = TubeTrace;
						PortalTrace2.surface = portalSimulator.Simulation.Static.SurfaceProperties.surface;
						PortalTrace2.contents = portalSimulator.Simulation.Static.SurfaceProperties.contents;
						PortalTrace2.m_pEnt = portalSimulator.Simulation.Static.SurfaceProperties.pEntity;
						TRACE_DEBUG_ONLY( traceDebugEntry.bCopiedRemoteTubeTrace = true );
					}
				}
#endif

				//if the remote portal trace yielded collision results, translate them back to local space
				if( PortalTrace2.startsolid || (ray_local.m_IsSwept && (PortalTrace2.fraction < PortalTrace.fraction)) )
				{					
#if defined( DBGFLAG_ASSERT )
					Vector vTranslatedStart = (pLinkedPortal->m_matrixThisToLinked * (PortalTrace2.startpos - vRemoteShift - ray_remote.m_StartOffset)) + ray_local.m_StartOffset;
					Assert( RealTrace.startsolid || ((vTranslatedStart - RealTrace.startpos).Length() < 0.1f) );
#endif

					//transformed trace hit something sooner than local trace. Have to respect it, we'd be hitting it if we teleported this frame
					PortalTrace = PortalTrace2;
					PortalTrace.m_pEnt = portalSimulator.Simulation.hCollisionEntity;
					
					PortalTrace.startpos = RealTrace.startpos;						
					PortalTrace.endpos = (pLinkedPortal->m_matrixThisToLinked * (PortalTrace2.endpos - vRemoteShift - ray_remote.m_StartOffset)) + ray_local.m_StartOffset;
					//PortalTrace.endpos = RealTrace.startpos + (ray_local.m_Delta * PortalTrace2.fraction);
					PortalTrace.plane.normal = pLinkedPortal->m_matrixThisToLinked.ApplyRotation( PortalTrace2.plane.normal );
					Vector vTranslatedPointOnPlane = pLinkedPortal->m_matrixThisToLinked * ((PortalTrace2.plane.normal * PortalTrace2.plane.dist) - vRemoteShift);
					PortalTrace.plane.dist = PortalTrace.plane.normal.Dot( vTranslatedPointOnPlane );

					TRACE_DEBUG_ONLY( traceDebugEntry.bCopiedPortal2Trace = true );

#if defined( DEBUG_PORTAL_TRACE_BOXES )
					debugBoxColor.b = 255;
					//NDebugOverlay::Box( (PortalTrace2.endpos - ray_remote.m_StartOffset - vRemoteShift), -ray_remote.m_Extents, ray_remote.m_Extents, 0, 0, 255, 100, 0.0f );
					//NDebugOverlay::Box( (pLinkedPortal->m_matrixThisToLinked * (PortalTrace2.endpos - ray_remote.m_StartOffset)), -ray_local.m_Extents, ray_local.m_Extents, 0, 0, 255, 100, 0.0f );
#endif
				}

				//verify that the portal trace is still the better option.
				//Several cases can fail this part
				// 1. We're simply "near" the portal and hitting remote collision geometry that overlaps our local space in non-euclidean ways
				// 2. The collision represention of the wall isn't as precise as we'd like and juts out a bit from the actual wall
				// 3. The translated portal tube or collision juts out from real space just a bit. If we' switch over to portal traces, we have to commit 100%
				if( RealTrace.startsolid || (!PortalTrace.startsolid && (ray_local.m_IsSwept && (PortalTrace.fraction >= RealTrace.fraction))) )
				{
					*((trace_t *)&pm) = PortalTrace;
					pm.m_bContactedPortalTransitionRamp = bContactedTransitionRamp;
				}
				else //portal trace results got crappy after incorporating AABB vs remote environment results, and real trace still valid
				{
					*((trace_t *)&pm) = RealTrace;
					pm.m_bContactedPortalTransitionRamp = false;
				}
			}
			else //real trace gives us better results
			{				
				*((trace_t *)&pm) = RealTrace;
				pm.m_bContactedPortalTransitionRamp = false;
			}

#if defined( DEBUG_PORTAL_TRACE_BOXES )
			if( sv_portal_new_trace_debugboxes.GetBool() )
			{
				NDebugOverlay::Box( ray_remote.m_Start, -ray_remote.m_Extents, ray_remote.m_Extents, debugBoxColor.r, debugBoxColor.g, debugBoxColor.b, 100, 0.0f );
				NDebugOverlay::Box( ray_local.m_Start, -ray_local.m_Extents, ray_local.m_Extents, debugBoxColor.r, debugBoxColor.g, debugBoxColor.b, 100, 0.0f );
			}
#endif
		}
		else //ray not touching portal hole
		{
			*((trace_t *)&pm) = RealTrace;
			pm.m_bContactedPortalTransitionRamp = false;
		}

		TRACE_DEBUG_ONLY( traceDebugEntry.finalTrace = *((trace_t *)&pm) );
	}
	else
	{
		//old trace behavior
		UTIL_Portal_TraceRay_With( pPortal, ray_local, fMask, &traceFilter, &pm );

		// If we're moving through a portal and failed to hit anything with the above ray trace
		// Use UTIL_Portal_TraceEntity to test this movement through a portal and override the trace with the result
		if ( pm.fraction == 1.0f && UTIL_DidTraceTouchPortals( ray_local, pm ) && sv_player_trace_through_portals.GetBool() )
		{
			trace_t tempTrace;
			Vector start = ray_local.m_Start + ray_local.m_StartOffset;
			UTIL_Portal_TraceEntity( pPortalPlayer, start, start + ray_local.m_Delta, fMask, &traceFilter, &tempTrace );

			if ( tempTrace.DidHit() && tempTrace.fraction < pm.fraction && !tempTrace.startsolid && !tempTrace.allsolid )
			{
				*((trace_t *)&pm) = tempTrace;
				pm.m_bContactedPortalTransitionRamp = false;
			}
		}
	}
}

void CPortalGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm )
{
	VPROF( "CGameMovement::TracePlayerBBox" );
	//UTIL_ClearTrace( pm );
	//memset( &pm, 0, sizeof( trace_t ) );
	
	CPortal_Player *pPortalPlayer = (CPortal_Player *)((CBaseEntity *)mv->m_nPlayerHandle.Get());
	CPortal_Base2D *pPortal = pPortalPlayer->m_hPortalEnvironment;

	Ray_t ray_local;
	ray_local.Init( start, end, GetPlayerMins(), GetPlayerMaxs() );

	if( pPortal && pPortal->IsActivedAndLinked() )
	{
		const PS_InternalData_t &portalSimulator = pPortal->m_PortalSimulator.GetInternalData();

		CPortal_Base2D *pLinkedPortal = pPortal->m_hLinkedPortal;
		const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

		Vector vTeleportExtents = ray_local.m_Extents; //if we would adjust our extents due to a portal teleport, that change should be reflected here

		//Need to take AABB extent changes from possible ducking into account
		//bool bForcedDuck = false;
		bool bDuckToFit = ShouldPortalTransitionCrouch( pPortal );
		bool bDuckToFling = ShouldMaintainFlingAssistCrouch( pLinkedPortal, pPortal->m_matrixThisToLinked.ApplyRotation( mv->m_vecVelocity ) );
		if( bDuckToFit || bDuckToFling )
		{
			//significant change in up axis
			//bForcedDuck = true;
			vTeleportExtents = (pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins()) * 0.5f;
		}

		Vector vRemoteShift = CalculateExtentShift( ray_local.m_Extents, portalSimulator.Placement.PortalPlane.m_Normal, vTeleportExtents, linkedPortalSimulator.Placement.PortalPlane.m_Normal );

		//this is roughly the ray we'd use if we had just teleported, the exception being the centered startoffset
		Ray_t ray_remote = ray_local;
		ray_remote.m_Start = (pPortal->m_matrixThisToLinked * ray_local.m_Start) + vRemoteShift;
		ray_remote.m_Delta = pPortal->m_matrixThisToLinked.ApplyRotation( ray_local.m_Delta );
		ray_remote.m_StartOffset = vec3_origin;
		//ray_remote.m_StartOffset.z -= vTeleportExtents.z;
		ray_remote.m_Extents = vTeleportExtents;		

		TracePortalPlayerAABB( pPortalPlayer, pPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bDuckToFling );
	}
	else
	{
		TracePortalPlayerAABB( pPortalPlayer, NULL, ray_local, ray_local, vec3_origin, fMask, collisionGroup, pm );
	}
}

void CPortalGameMovement::TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm )
{
	CTrace_PlayerAABB_vs_Portals pm2;
	*(trace_t *)&pm2 = pm;
	TracePlayerBBox( start, end, fMask, collisionGroup, pm2 );
	pm = *(trace_t *)&pm2;
}



void PortalTracePlayerBBoxForGround( const Vector& start, const Vector& end, const Vector& minsSrc,
									const Vector& maxsSrc, IHandleEntity *player, unsigned int fMask,
									int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm, const Vector& vStickNormal )
{
	VPROF( "PortalTracePlayerBBoxForGround" );

	CPortal_Player *pPortalPlayer = dynamic_cast<CPortal_Player *>(player->GetRefEHandle().Get());
	CPortal_Base2D *pPlayerPortal = pPortalPlayer->m_hPortalEnvironment;
	
	pm.m_bContactedPortalTransitionRamp = false;

#ifndef CLIENT_DLL
	if( pPlayerPortal && pPlayerPortal->m_PortalSimulator.IsReadyToSimulate() == false )
		pPlayerPortal = NULL;
#endif

	Vector vTransformedStart, vTransformedEnd;
	Vector transformed_minsSrc, transformed_maxsSrc;
	Vector vRemoteShift = vec3_origin;
	bool bSkipRemoteTube = false;
	if( pPlayerPortal && pPlayerPortal->IsActivedAndLinked() && sv_portal_new_player_trace.GetBool() )
	{
		const PS_InternalData_t &portalSimulator = pPlayerPortal->m_PortalSimulator.GetInternalData();

		CPortal_Base2D *pLinkedPortal = pPlayerPortal->m_hLinkedPortal;
		const PS_InternalData_t &linkedPortalSimulator = pLinkedPortal->m_PortalSimulator.GetInternalData();

		Vector vOriginToCenter = (maxsSrc + minsSrc) * 0.5f;
		vTransformedStart = pPlayerPortal->m_matrixThisToLinked * (start + vOriginToCenter);
		vTransformedEnd = pPlayerPortal->m_matrixThisToLinked * (end + vOriginToCenter);
		transformed_minsSrc = minsSrc;
		transformed_maxsSrc = maxsSrc;

		bool bDuckToFit = ShouldPortalTransitionCrouch( pPlayerPortal );
		bool bDuckToFling = ShouldMaintainFlingAssistCrouch( pLinkedPortal, pLinkedPortal->m_matrixThisToLinked.ApplyRotation( pPortalPlayer->GetAbsVelocity() ) );
		//Need to take AABB extent changes from possible ducking into account
		
		if( bDuckToFit || bDuckToFling )
		{
			//significant change in up axis
			transformed_minsSrc = pPortalPlayer->GetDuckHullMins();
			transformed_maxsSrc = pPortalPlayer->GetDuckHullMaxs();
			bSkipRemoteTube = bDuckToFling;
		}

		Vector vLocalExtents = (maxsSrc - minsSrc) * 0.5f;
		Vector vTeleportExtents = (transformed_maxsSrc - transformed_minsSrc) * 0.5f;

		vRemoteShift = CalculateExtentShift( vLocalExtents, portalSimulator.Placement.PortalPlane.m_Normal, vTeleportExtents, linkedPortalSimulator.Placement.PortalPlane.m_Normal );
		
		//just recompute origin offset for extent shifting again in case it changed
		vOriginToCenter = (transformed_maxsSrc + transformed_minsSrc) * 0.5f;
		//transformed_minsSrc -= vOriginToCenter;
		//transformed_maxsSrc -= vOriginToCenter;
		vTransformedStart -= vOriginToCenter;
		vTransformedEnd -= vOriginToCenter;
	}
	else
	{
		vTransformedStart = start;
		vTransformedEnd = end;
		transformed_minsSrc = minsSrc;
		transformed_maxsSrc = maxsSrc;
	}

	Ray_t ray_local, ray_remote;
	Vector mins, maxs;

	float fraction = pm.fraction;
	Vector endpos = pm.endpos;

	// Check the -x, -y quadrant
	mins = minsSrc;
	maxs.Init( MIN( 0, maxsSrc.x ), MIN( 0, maxsSrc.y ), maxsSrc.z );
	ray_local.Init( start, end, mins, maxs );
	mins = transformed_minsSrc;
	maxs.Init( MIN( 0, transformed_maxsSrc.x ), MIN( 0, transformed_maxsSrc.y ), transformed_maxsSrc.z );
	ray_remote.Init( vTransformedStart, vTransformedEnd, mins, maxs );
	ray_remote.m_Start += vRemoteShift;
	ray_remote.m_StartOffset += vRemoteShift;

	

	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	float flNormalCos = DotProduct( pm.plane.normal, vStickNormal );
	if ( pm.m_pEnt && ((flNormalCos >= 0.7f) || pm.HitPortalRamp(Vector( 0.0f, 0.0f, 1.0f ))) )
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the +x, +y quadrant
	mins.Init( MAX( 0, minsSrc.x ), MAX( 0, minsSrc.y ), minsSrc.z );
	maxs = maxsSrc;
	ray_local.Init( start, end, mins, maxs );
	mins.Init( MAX( 0, transformed_minsSrc.x ), MAX( 0, transformed_minsSrc.y ), transformed_minsSrc.z );
	maxs = transformed_maxsSrc;
	ray_remote.Init( vTransformedStart, vTransformedEnd, mins, maxs );
	ray_remote.m_Start += vRemoteShift;
	ray_remote.m_StartOffset += vRemoteShift;

	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	flNormalCos = DotProduct( pm.plane.normal, vStickNormal );
	if ( pm.m_pEnt && ((flNormalCos >= 0.7f) || pm.HitPortalRamp(Vector( 0.0f, 0.0f, 1.0f ))) )
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the -x, +y quadrant
	mins.Init( minsSrc.x, MAX( 0, minsSrc.y ), minsSrc.z );
	maxs.Init( MIN( 0, maxsSrc.x ), maxsSrc.y, maxsSrc.z );
	ray_local.Init( start, end, mins, maxs );
	mins.Init( transformed_minsSrc.x, MAX( 0, transformed_minsSrc.y ), transformed_minsSrc.z );
	maxs.Init( MIN( 0, transformed_maxsSrc.x ), transformed_maxsSrc.y, transformed_maxsSrc.z );
	ray_remote.Init( vTransformedStart, vTransformedEnd, mins, maxs );
	ray_remote.m_Start += vRemoteShift;
	ray_remote.m_StartOffset += vRemoteShift;

	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	flNormalCos = DotProduct( pm.plane.normal, vStickNormal );
	if ( pm.m_pEnt && ((flNormalCos >= 0.7f) || pm.HitPortalRamp(Vector( 0.0f, 0.0f, 1.0f ))) )
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	// Check the +x, -y quadrant
	mins.Init( MAX( 0, minsSrc.x ), minsSrc.y, minsSrc.z );
	maxs.Init( maxsSrc.x, MIN( 0, maxsSrc.y ), maxsSrc.z );
	ray_local.Init( start, end, mins, maxs );
	mins.Init( MAX( 0, transformed_minsSrc.x ), transformed_minsSrc.y, transformed_minsSrc.z );
	maxs.Init( transformed_maxsSrc.x, MIN( 0, transformed_maxsSrc.y ), transformed_maxsSrc.z );
	ray_remote.Init( vTransformedStart, vTransformedEnd, mins, maxs );
	ray_remote.m_Start += vRemoteShift;
	ray_remote.m_StartOffset += vRemoteShift;

	if( pPlayerPortal )
	{
		if( sv_portal_new_player_trace.GetBool() )
		{
			TracePortalPlayerAABB( pPortalPlayer, pPlayerPortal, ray_local, ray_remote, vRemoteShift, fMask, collisionGroup, pm, bSkipRemoteTube );
		}
		else
		{
			UTIL_Portal_TraceRay( pPlayerPortal, ray_local, fMask, player, collisionGroup, &pm );
		}
	}
	else
	{
		UTIL_TraceRay( ray_local, fMask, player, collisionGroup, &pm );
	}

	flNormalCos = DotProduct( pm.plane.normal, vStickNormal );
	if ( pm.m_pEnt && ((flNormalCos >= 0.7f) || pm.HitPortalRamp(Vector( 0.0f, 0.0f, 1.0f ))) )
	{
		pm.fraction = fraction;
		pm.endpos = endpos;
		return;
	}

	pm.fraction = fraction;
	pm.endpos = endpos;
}

CBaseHandle CPortalGameMovement::TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm )
{
	TracePlayerBBox( pos, pos, MASK_PLAYERSOLID, collisionGroup, pm ); //hook into the existing portal special trace functionality

	//Ray_t ray;
	//ray.Init( pos, pos, GetPlayerMins(), GetPlayerMaxs() );
	//UTIL_TraceRay( ray, MASK_PLAYERSOLID, mv->m_nPlayerHandle.Get(), collisionGroup, &pm );
	if( pm.startsolid && pm.m_pEnt && (pm.contents & MASK_PLAYERSOLID) )
	{
#ifdef _DEBUG
		Warning( "The player got stuck on something. Break in portal_gamemovement.cpp on line " __LINE__AS_STRING " to investigate\n" ); //happens enough to just leave in a perma-debugger
		//now that you're here, go up and re-run the TracePlayerBBox() above
#endif
#if defined( TRACE_DEBUG_ENABLED )
		g_bTraceDebugging = true;
		TracePlayerBBox( pos, pos, MASK_PLAYERSOLID, collisionGroup, pm ); //hook into the existing portal special trace functionality
#endif
		return pm.m_pEnt->GetRefEHandle();
	}
#ifndef CLIENT_DLL
	else if ( pm.startsolid && pm.m_pEnt && CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pm.m_pEnt ) )
	{
		// Stuck in a portal environment object, so unstick them!
		CPortal_Player *pPortalPlayer = (CPortal_Player *)((CBaseEntity *)mv->m_nPlayerHandle.Get());
		pPortalPlayer->SetStuckOnPortalCollisionObject();

		return INVALID_EHANDLE_INDEX;
	}
#endif
	else
	{	
		return INVALID_EHANDLE_INDEX;
	}
}


void CPortalGameMovement::HandlePortalling( void )
{
	matrix3x4_t matAngleTransformIn, matAngleTransformOut; //temps for angle transformation
	CPortal_Player *pPortalPlayer = ToPortalPlayer( player );

#if ( PLAYERPORTALDEBUGSPEW == 1 )
#	if defined( CLIENT_DLL )
	const char *szDLLName = "client";
#	else
	const char *szDLLName = "--server--";
#	endif
#endif

#if defined( CLIENT_DLL )	
	pPortalPlayer->UnrollPredictedTeleportations( player->m_pCurrentCommand->command_number );
	pPortalPlayer->CheckPlayerAboutToTouchPortal();
#endif

	Vector vOriginToCenter = (pPortalPlayer->GetHullMaxs() + pPortalPlayer->GetHullMins()) * 0.5f;
	Vector vMoveCenter = mv->GetAbsOrigin() + vOriginToCenter;
	Vector vPrevMoveCenter = m_vMoveStartPosition + vOriginToCenter; //whatever the origin was before we ran this move
	Vector vPlayerExtentsFromCenter = (pPortalPlayer->GetHullMaxs() - pPortalPlayer->GetHullMins()) * 0.5f;
	//Vector vAppliedMoveCenter = pPortalPlayer->GetAbsOrigin() + vOriginToCenter;

#if defined( TRACE_DEBUG_ENABLED )
	CheckStuck();
#endif

#if defined( DEBUG_FLINGS )
	if( pPortalPlayer->GetGroundEntity() != NULL )
	{
		s_MovementDebug.bWatchFlingVelocity = false;
	}
#endif

	CPortal_Base2D *pPortalEnvironment = pPortalPlayer->m_hPortalEnvironment.Get(); //the portal they were touching last move
	CPortal_Base2D *pPortal = NULL;
	{
		Ray_t ray;
		ray.Init( m_vMoveStartPosition, mv->GetAbsOrigin(), pPortalPlayer->GetHullMins(), pPortalPlayer->GetHullMaxs() );
		//ray.Init( mv->GetAbsOrigin(), mv->GetAbsOrigin(), pPortalPlayer->GetHullMins(), pPortalPlayer->GetHullMaxs() );
		
		int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
		CPortal_Base2D **pAllPortals = CPortal_Base2D_Shared::AllPortals.Base();

		float fMaxDistSquared = FLT_MAX;
		for( int i = 0; i != iPortalCount; ++i )
		{
			if( pAllPortals[i]->IsActivedAndLinked() )
			{
				trace_t tr;
				UTIL_ClearTrace( tr );
				if( pAllPortals[i]->TestCollision( ray, CONTENTS_SOLID, tr ) )
				{
					const PS_InternalData_t &portalSimulator = pAllPortals[i]->m_PortalSimulator.GetInternalData();
					if( portalSimulator.Placement.PortalPlane.m_Normal.Dot( vPrevMoveCenter ) <= portalSimulator.Placement.PortalPlane.m_Dist )
					{
						//old origin must be in front of the plane

						if( pAllPortals[i] != pPortalEnvironment ) //special exception if we were pushed past the plane but did not move past it
							continue;
					}

					float fCenterDist = portalSimulator.Placement.PortalPlane.m_Normal.Dot( vMoveCenter ) - portalSimulator.Placement.PortalPlane.m_Dist;
					if( fCenterDist < 0.0f )
					{
						//if new origin is behind the plane, require that the player center hover over the portal quad to be considered
						Vector vPortalPlayerOriginDiff = vMoveCenter - portalSimulator.Placement.ptCenter;
						vPortalPlayerOriginDiff -= vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.PortalPlane.m_Normal ) * portalSimulator.Placement.PortalPlane.m_Normal;

						if( (fabs( vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.vRight ) ) > portalSimulator.Placement.fHalfWidth) ||
							(fabs( vPortalPlayerOriginDiff.Dot( portalSimulator.Placement.vUp ) ) > portalSimulator.Placement.fHalfHeight) )
						{
							continue;
						}
					}
					else
					{
						//require that a line from the center of the player to their most-penetrating extent passes through the portal quad
						//Avoids case where you can butt up against a portal side on an angled panel
						Vector vTestExtent = vMoveCenter;
						vTestExtent.x -= Sign( portalSimulator.Placement.PortalPlane.m_Normal.x ) * vPlayerExtentsFromCenter.x;
						vTestExtent.y -= Sign( portalSimulator.Placement.PortalPlane.m_Normal.y ) * vPlayerExtentsFromCenter.y;
						vTestExtent.z -= Sign( portalSimulator.Placement.PortalPlane.m_Normal.z ) * vPlayerExtentsFromCenter.z;

						float fTestDist = portalSimulator.Placement.PortalPlane.m_Normal.Dot( vTestExtent ) - portalSimulator.Placement.PortalPlane.m_Dist;

						if( fTestDist < portal_player_interaction_quadtest_epsilon.GetFloat() )
						{
							float fTotalDist = fCenterDist - fTestDist;
							if( fTotalDist != 0.0f )
							{
								Vector vPlanePoint = (vTestExtent * (fCenterDist/fTotalDist)) - (vMoveCenter * (fTestDist/fTotalDist));
								Vector vPortalCenterToPlanePoint = vPlanePoint - portalSimulator.Placement.ptCenter;

								if( (fabs( vPortalCenterToPlanePoint.Dot( portalSimulator.Placement.vRight ) ) > portalSimulator.Placement.fHalfWidth + 1.0f) ||
									(fabs( vPortalCenterToPlanePoint.Dot( portalSimulator.Placement.vUp ) ) > portalSimulator.Placement.fHalfHeight + 1.0f) )
								{
									continue;
								}
							}
						}
					}


					Vector vDiff = pAllPortals[i]->m_ptOrigin - vMoveCenter;
					float fDistSqr = vDiff.LengthSqr();
					if( fDistSqr < fMaxDistSquared )
					{
						pPortal = pAllPortals[i];
						fMaxDistSquared = fDistSqr;
					}
				}
			}
		}
	}
	//CPortal_Base2D *pPortal = UTIL_IntersectEntityExtentsWithPortal( player );
	CPortal_Base2D *pPlayerTouchingPortal = pPortal;

	if( pPortal != NULL )
	{
		CPortal_Base2D *pExitPortal = pPortal->m_hLinkedPortal;
		bool bMobile = pPortal->IsMobile() || pPortal->m_hLinkedPortal->IsMobile();
		float fPlaneDist = pPortal->m_plane_Origin.normal.Dot( vMoveCenter ) - pPortal->m_plane_Origin.dist;
		if( (fPlaneDist < -FLT_EPSILON) || //behind the portal plane
			(bMobile && (pPortal->m_plane_Origin.normal.Dot( mv->m_vecVelocity ) < -FLT_EPSILON))  ) //trying to enter a mobile portal
		{

			// Capture our eye position before we start portalling
			Vector vOldEyePos = mv->GetAbsOrigin() + pPortalPlayer->GetViewOffset();

#if (PLAYERPORTALDEBUGSPEW == 1)
			bool bStartedDucked = ((player->GetFlags() & FL_DUCKING) != 0);
#endif

			//compute when exactly we intersected the portal's plane
			float fIntersectionPercentage;
			float fPostPortalFrameTime;
			{
				float fOldPlaneDist = pPortal->m_plane_Origin.normal.Dot( vPrevMoveCenter ) - pPortal->m_plane_Origin.dist;
				float fTotalDist = (fOldPlaneDist - fPlaneDist); //fPlaneDist is known to be negative
				fIntersectionPercentage = (fTotalDist != 0.0f) ? (fOldPlaneDist / fTotalDist) : 0.5f; //but sometimes fOldPlaneDist is too, some kind of physics penetration seems to be the cause (bugbait #61331)
				fPostPortalFrameTime = (1.0f - fIntersectionPercentage ) * gpGlobals->frametime;
			}

#if 0		//debug spew
#	if defined( CLIENT_DLL )
			if( pPortalPlayer->m_PredictedPortalTeleportations.Count() > 1 )
			{
				Warning( "==================CPortalGameMovement::HandlePortalling(client) has built up more than 1 teleportation=====================\n" );
			}
#	endif
#endif

#if defined( GAME_DLL )
			{
				pPortal->PreTeleportTouchingEntity( player );

				if ( pPortalPlayer->IsUsingVMGrab() )
				{
					CBaseEntity *pEntity = GetPlayerHeldEntity( pPortalPlayer );
					if ( pEntity && pPortal )
					{
						Vector vCurPos = pEntity->GetLocalOrigin();
						QAngle vCurAngles = pEntity->GetLocalAngles();
						Vector vNewPos;
						QAngle vNewAngles;
						UTIL_Portal_PointTransform( pPortal->MatrixThisToLinked(), vCurPos, vNewPos );
						UTIL_Portal_AngleTransform( pPortal->MatrixThisToLinked(), vCurAngles, vNewAngles );

						pEntity->Teleport( &vNewPos, &vNewAngles, NULL );
					}
				}

				// Notify the entity that it's being teleported
				// Tell the teleported entity of the portal it has just arrived at
				notify_teleport_params_t paramsTeleport;
				paramsTeleport.prevOrigin		= mv->GetAbsOrigin();
				paramsTeleport.prevAngles		= mv->m_vecViewAngles;
				paramsTeleport.physicsRotate	= true;
				notify_system_event_params_t eventParams ( &paramsTeleport );
				player->NotifySystemEvent( pPortal, NOTIFY_EVENT_TELEPORT, eventParams );
			}
			
#if 0		//debug info for which portal teleported me
			{
				NDebugOverlay::EntityBounds( pPortal, 255, 0, 0, 100, 10.0f ); //portal that's teleporting us in red
				NDebugOverlay::Box( pPortalPlayer->GetPreviouslyPredictedOrigin(), pPortalPlayer->GetHullMins(), pPortalPlayer->GetHullMaxs(), 0, 0, 255, 100, 10.0f ); //last player position in blue
				NDebugOverlay::Box( mv->GetAbsOrigin(), pPortalPlayer->GetHullMins(), pPortalPlayer->GetHullMaxs(), 0, 255, 0, 100, 10.0f ); //current player position in green
				NDebugOverlay::Box( (vOldMoveCenter * (1.0f - fIntersectionPercentage)) + (vMoveCenter * fIntersectionPercentage), -Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ), 0, 255, 255, 255, 10.0f );
			}
#endif
#endif


			matrix3x4_t matTransform = pPortal->MatrixThisToLinked().As3x4();
			bool bWasOnGround = (player->GetFlags() & FL_ONGROUND) != 0;

			player->SetGroundEntity( NULL );

			//velocity
			{
				Vector vPostPortalGravityVelocity = bWasOnGround ? vec3_origin : 
							(m_vGravityDirection * (sv_gravity.GetFloat() * fPostPortalFrameTime * ((player->GetGravity() != 0) ? player->GetGravity() : 1.0f)));
				Vector vTransformedVelocity;
				Vector vVelocity = mv->m_vecVelocity;

				//subtract out gravity for the portion of the frame after we portalled.				
				vVelocity -= vPostPortalGravityVelocity;

				// Take the implicit vertical speed we get when walking along a sloped surface into account,
				// since player velocity is only ever in the xy-plane while on the ground.
				// Ignore this if we're entering a floor portal.
				const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians
				if( pPortal->m_plane_Origin.normal.z <= COS_PI_OVER_SIX )
				{
#				ifdef PORTAL2
					vVelocity += pPortalPlayer->GetImplicitVerticalStepSpeed() * pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal;
#				else
					vVelocity.z += pPortalPlayer->GetImplicitVerticalStepSpeed();
#				endif
				}

				VectorRotate( vVelocity, matTransform, vTransformedVelocity );

				//now add gravity for the post-portal portion of the frame back on, but to the transformed velocity
				vTransformedVelocity += vPostPortalGravityVelocity * 1.008f; //Apply slightly more gravity on exit so that floor/floor portals trend towards decaying velocity. 1.008 is a magic number found through experimentation

				//velocity hacks
				{
					float fExitMin, fExitMax;
					Vector vTransformedMoveCenter;
					VectorTransform( vMoveCenter, matTransform, vTransformedMoveCenter );
					CPortal_Base2D::GetExitSpeedRange( pPortal, true, fExitMin, fExitMax, vTransformedMoveCenter, player );

					float fForwardSpeed = vTransformedVelocity.Dot( pExitPortal->m_vForward );										
					if( fForwardSpeed < fExitMin )
					{
						vTransformedVelocity += pExitPortal->m_vForward * (fExitMin - fForwardSpeed);
					}
					else
					{
						float fSpeed = vTransformedVelocity.Length();
						if( (fSpeed > fExitMax) && (fSpeed != 0.0f) )
						{
							vTransformedVelocity *= (fExitMax / fSpeed);
						}
					}
				}

				//cap it to the same absolute maximum that CGameMovement::CheckVelocity() would, but be quiet about it.
				const float fAbsoluteMaxVelocity = sv_maxvelocity.GetFloat();
				for( int i = 0; i != 3; ++i )
				{
					if (vTransformedVelocity[i] > fAbsoluteMaxVelocity)
					{
						vTransformedVelocity[i] = fAbsoluteMaxVelocity;
					}
					else if (vTransformedVelocity[i] < -fAbsoluteMaxVelocity)
					{
						vTransformedVelocity[i] = -fAbsoluteMaxVelocity;
					}
				}

				mv->m_vecVelocity = vTransformedVelocity;
			}

		/*	pPlayerTouchingPortal->TransformPlayerTeleportingPlayerOrigin( pPortalPlayer, vMoveCenter, vOriginToCenter, mv->m_vecVelocity );
			mv->SetAbsOrigin( vMoveCenter );*/


			bool bForceDuckToFit = ShouldPortalTransitionCrouch( pPlayerTouchingPortal );
			bool bForceDuckToFlingAssist = ShouldMaintainFlingAssistCrouch( pPlayerTouchingPortal->m_hLinkedPortal, mv->m_vecVelocity );
			bool bForceDuck = bForceDuckToFit || bForceDuckToFlingAssist;

			//reduced version of forced duckjump
			if( bForceDuck )
			{
				if( (player->GetFlags() & FL_DUCKING) == 0 ) //not already ducking
				{
					//DevMsg( "HandlePortalling: Force Duck\n" );
					player->m_Local.m_bInDuckJump = true;
					player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
					FinishDuck(); //be ducked RIGHT NOW. Pulls feet up by delta between hull sizes.

					Vector vNewOriginToCenter = (pPortalPlayer->GetDuckHullMaxs() + pPortalPlayer->GetDuckHullMins()) * 0.5f;
					//Vector vNewCenter = mv->GetAbsOrigin() + vNewOriginToCenter;
					//mv->SetAbsOrigin( mv->GetAbsOrigin() + (vMoveCenter - vNewCenter) ); //We need our center to where it started
					vOriginToCenter = vNewOriginToCenter;
				}
			}

			//transform by center
			{
#if defined( DEBUG_FLINGS )
				s_MovementDebug.vCenterNudge = vec3_origin;
#endif

				Vector vTransformedMoveCenter;
				VectorTransform( vMoveCenter, matTransform, vTransformedMoveCenter );
				CPortal_Base2D *pExitPortal = pPortal->m_hLinkedPortal;
				if( bMobile )
				{
					Vector vExtents = (pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins()) * 0.5f;
					const Vector &vPushNormal = pExitPortal->m_plane_Origin.normal;
					float fAgreeableDot = fabs(vPushNormal.x * vExtents.x) + fabs(vPushNormal.y * vExtents.y) + fabs(vPushNormal.z * vExtents.z);
					vTransformedMoveCenter = pExitPortal->m_ptOrigin + (pExitPortal->m_plane_Origin.normal * fAgreeableDot);
				}
				else
				{
					if( bForceDuckToFlingAssist || (bForceDuckToFit && (mv->m_vecVelocity.Dot( pPlayerTouchingPortal->m_hLinkedPortal->m_vForward ) > PLAYER_FLING_HELPER_MIN_SPEED)) )
					{
						Vector vExtents = (pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins()) * 0.5f;

						//HACK: people like to fling through portals, unfortunately their bbox changes enough to cause problems on fling exit
						//the real world equivalent of stubbing your toe on the exit hole results in flinging straight up.
						//The hack is to fix this by preventing you from stubbing your toe, move the player towards portal center axis
						Vector vPortalToCenter = vTransformedMoveCenter - pExitPortal->m_ptOrigin;
						Vector vOffCenter = vPortalToCenter - (vPortalToCenter.Dot( pExitPortal->m_vForward ) * pExitPortal->m_vForward);

						//find the extent which is most likely to get hooked on the edge
						Vector vTestPoint = vTransformedMoveCenter; //pExitPortal->m_ptOrigin + vOffCenter;
						for( int k = 0; k != 3; ++k )
						{
							vTestPoint[k] += vExtents[k] * Sign( vOffCenter[k] );
						}
						Vector vPortalToTest = vTestPoint - pExitPortal->m_ptOrigin;
						//project it onto the portal plane
						//vPortalToTest = vPortalToTest - (vPortalToTest.Dot( pExitPortal->m_vForward ) * pExitPortal->m_vForward);

						float fRight = vPortalToTest.Dot( pExitPortal->m_vRight );
						float fUp = vPortalToTest.Dot( pExitPortal->m_vUp );

						Vector vNudge = vec3_origin;
						float fNudgeWidth = pExitPortal->GetHalfWidth() - 5.0f;
						float fNudgeHeight = pExitPortal->GetHalfHeight() - 5.0f;
						if( fRight > fNudgeWidth )
						{
							vNudge -= pExitPortal->m_vRight * (fRight - fNudgeWidth); 
						}
						else if( fRight < -fNudgeWidth )
						{
							vNudge -= pExitPortal->m_vRight * (fRight + fNudgeWidth);
						}

						if( fUp > fNudgeHeight )
						{
							vNudge -= pExitPortal->m_vUp * (fUp - fNudgeHeight); 
						}
						else if( fUp < -fNudgeHeight )
						{
							vNudge -= pExitPortal->m_vUp * (fUp + fNudgeHeight);
						}

						vTransformedMoveCenter += vNudge;

#if defined( DEBUG_FLINGS )
						s_MovementDebug.vCenterNudge = vNudge;
#endif						
					}
				}

				mv->SetAbsOrigin( vTransformedMoveCenter - vOriginToCenter );

#if defined( CLIENT_DLL )
				{
					C_Portal_Player::PredictedPortalTeleportation_t entry;
					entry.flTime = gpGlobals->curtime;
					entry.pEnteredPortal = pPortal;
					entry.iCommandNumber = player->m_pCurrentCommand->command_number;
					entry.fDeleteServerTimeStamp = -1.0f;
					entry.matUnroll = pPortal->m_hLinkedPortal->MatrixThisToLinked();
					entry.bDuckForced = bForceDuck;
					pPortalPlayer->m_PredictedPortalTeleportations.AddToTail( entry );
				}		
#endif

#	if ( PLAYERPORTALDEBUGSPEW == 1 )
				Warning( "-->CPortalGameMovement::HandlePortalling(%s) teleport player, portal %d, time %f, %d, Start Center: %.2f %.2f %.2f (%s)  End: %.2f %.2f %.2f (%s)\n", szDLLName, pPortal->m_bIsPortal2 ? 2 : 1, gpGlobals->curtime, player->m_pCurrentCommand->command_number, XYZ( vMoveCenter ), bStartedDucked ? "duck":"stand", XYZ( vTransformedMoveCenter ), ((player->GetFlags() & FL_DUCKING) != 0) ? "duck":"stand" );
#	endif
			}

#if defined( CLIENT_DLL )
			//engine view angles (for mouse input smoothness)
			{
				QAngle qEngineAngles;
				engine->GetViewAngles( qEngineAngles );
				AngleMatrix( qEngineAngles, matAngleTransformIn );
				ConcatTransforms( matTransform, matAngleTransformIn, matAngleTransformOut );
				MatrixAngles( matAngleTransformOut, qEngineAngles );
				engine->SetViewAngles( qEngineAngles );
			}

			//predicted view angles
			{
				QAngle qPredViewAngles;
				prediction->GetViewAngles( qPredViewAngles );
				
				AngleMatrix( qPredViewAngles, matAngleTransformIn );
				ConcatTransforms( matTransform, matAngleTransformIn, matAngleTransformOut );
				MatrixAngles( matAngleTransformOut, qPredViewAngles );

				prediction->SetViewAngles( qPredViewAngles );
			}

#endif

			//pl.v_angle
			{
				AngleMatrix( player->pl.v_angle, matAngleTransformIn );
				ConcatTransforms( matTransform, matAngleTransformIn, matAngleTransformOut );
				MatrixAngles( matAngleTransformOut, player->pl.v_angle );
			}

			//player entity angle
			{
				QAngle qPlayerAngle;
#if defined( GAME_DLL )
				qPlayerAngle = player->GetAbsAngles();
#else
				qPlayerAngle = player->GetNetworkAngles();
#endif
				AngleMatrix( qPlayerAngle, matAngleTransformIn );
				ConcatTransforms( matTransform, matAngleTransformIn, matAngleTransformOut );
				MatrixAngles( matAngleTransformOut, qPlayerAngle );

#if defined( GAME_DLL )
				player->SetAbsAngles( qPlayerAngle );
#else
				player->SetNetworkAngles( qPlayerAngle );
#endif
			}

#if defined( GAME_DLL )
			//outputs that the portal usually fires
			{
				// Update the portal to know something moved through it
				pPortal->OnEntityTeleportedFromPortal( player );
				if ( pPortal->m_hLinkedPortal )
				{
					pPortal->m_hLinkedPortal->OnEntityTeleportedToPortal( player );
				}
			}

			//notify clients of the teleportation
			EntityPortalled( pPortal, player, mv->GetAbsOrigin(), mv->m_vecAngles, bForceDuck );
#endif

#if defined( DEBUG_FLINGS )
			//movement debugging
			{
				Vector vHullMins = pPortalPlayer->GetHullMins();
				Vector vHullMaxs = pPortalPlayer->GetHullMaxs();
				Vector vHullExtents = (vHullMaxs - vHullMins) * 0.5f;
				Vector vHullCenter = mv->GetAbsOrigin() + ((vHullMaxs + vHullMins) * 0.5f);
				s_MovementDebug.pPlayer = pPortalPlayer;
				s_MovementDebug.pPortal = pPortal;
				s_MovementDebug.vTeleportPos = vHullCenter;
				s_MovementDebug.vTeleportExtents = vHullExtents;
				s_MovementDebug.vTeleportVel = mv->m_vecVelocity;
				s_MovementDebug.bWatchFlingVelocity = true;
			}
#endif

			pPlayerTouchingPortal = pPortal->m_hLinkedPortal.Get();

#if defined( GAME_DLL )
			pPortalPlayer->ApplyPortalTeleportation( pPortal, mv );
			pPortal->PostTeleportTouchingEntity( player );
#else
			pPortalPlayer->ApplyPredictedPortalTeleportation( pPortal, mv, bForceDuck );		
#endif

			//Fix us up if we got stuck
			//if( sv_portal_new_player_trace.GetBool() )
			{
				CPortal_Base2D *pPortalEnvBackup = pPortalPlayer->m_hPortalEnvironment.Get();
				pPortalPlayer->m_hPortalEnvironment = pPlayerTouchingPortal; //we need to trace against the new environment now instead of waiting for it to update naturally.
				trace_t portalTrace;
				TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, portalTrace );

				if( portalTrace.startsolid )
				{
					Vector vPlayerCenter = mv->GetAbsOrigin() + vOriginToCenter;
					Vector vPortalToCenter = vPlayerCenter - pPlayerTouchingPortal->m_ptOrigin;
					Vector vPortalNormal = pPlayerTouchingPortal->m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Normal;
					Vector vOffCenter = vPortalToCenter - (vPortalToCenter.Dot( vPortalNormal ) * vPortalNormal);
					//AABB's going through portals are likely to cause weird collision bugs. Just try to get them close
					TracePlayerBBox( mv->GetAbsOrigin() - vOffCenter, mv->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, portalTrace );
					if( !portalTrace.startsolid )
					{
						mv->SetAbsOrigin( portalTrace.endpos );
					}
					else
					{
						Vector vNewCenter = vec3_origin;
						Vector vExtents = (pPortalPlayer->GetHullMaxs() - pPortalPlayer->GetHullMins()) * 0.5f;
						CTraceFilterSimple traceFilter( pPortalPlayer, COLLISION_GROUP_PLAYER_MOVEMENT );

						if( UTIL_FindClosestPassableSpace_InPortal_CenterMustStayInFront( pPlayerTouchingPortal, vPlayerCenter, vExtents, pPlayerTouchingPortal->m_plane_Origin.normal, &traceFilter, MASK_PLAYERSOLID, 100, vNewCenter ) &&
							((pPlayerTouchingPortal->m_plane_Origin.normal.Dot( vNewCenter ) - pPlayerTouchingPortal->m_plane_Origin.dist) >= 0.0f) )
						{
							TracePlayerBBox( vNewCenter - vOriginToCenter, mv->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, portalTrace );
							if( !portalTrace.startsolid )
							{
								mv->SetAbsOrigin( portalTrace.endpos );
							}
							else
							{
								mv->SetAbsOrigin( vNewCenter - vOriginToCenter );
							}
						}						
					}
				}

				pPortalPlayer->m_hPortalEnvironment = pPortalEnvBackup;

				// Transform our old eye position through the portals so we can set an offset of where our eye
				// would have been and where it actually is
				Vector vTransformedEyePos;
				VectorTransform( vOldEyePos, pPortal->m_matrixThisToLinked.As3x4(), vTransformedEyePos );
				Vector vNewEyePos = mv->GetAbsOrigin() + pPortalPlayer->GetViewOffset();
				pPortalPlayer->SetEyeOffset( vTransformedEyePos, vNewEyePos );

			}
		}
#if ( PLAYERPORTALDEBUGSPEW == 1 ) && 0 //debugging spew
		else
		{
			static float fMaxTime = 0.0f; //prediction can produce lots and lots and lots of spew if we allow it to repeat itself
			if( fMaxTime < gpGlobals->curtime )
			{
				Warning( "CPortalGameMovement::HandlePortalling(%s) %f player touching portal %i without teleporting  dist %f vel %f\n", szDLLName, gpGlobals->curtime, pPortal->m_bIsPortal2 ? 2 : 1,
							(pPortal->m_plane_Origin.normal.Dot( vMoveCenter ) - pPortal->m_plane_Origin.dist), pPortal->m_plane_Origin.normal.Dot( mv->m_vecVelocity ) );
			
				fMaxTime = gpGlobals->curtime;
			}
		}
#endif

		if( bMobile )
		{
			pPlayerTouchingPortal = NULL;
		}
	}


	
	pPortalEnvironment = pPortalPlayer->m_hPortalEnvironment.Get();

	if( pPlayerTouchingPortal != pPortalEnvironment )
	{
		if( pPortalEnvironment )
		{
			pPortalEnvironment->m_PortalSimulator.ReleaseOwnershipOfEntity( player );
		}

		pPortalPlayer->m_hPortalEnvironment = pPlayerTouchingPortal;
		if( pPlayerTouchingPortal )
		{
			pPlayerTouchingPortal->m_PortalSimulator.TakeOwnershipOfEntity( player );
		}
		else
		{
			//we're switching out the portal collision for real collision. Unlike entering from the real world to the portal, we can't wait for it to opportunistically
			//find a non-stuck case to transition. Fix it now if the player is stuck (so far it's always been a floating point precision problem)
			trace_t realTrace;
			TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, realTrace );
			if( realTrace.startsolid )
			{
				//crap
				trace_t portalTrace;
				pPortalPlayer->m_hPortalEnvironment = pPortalEnvironment; //try it again with the old environment to be sure
				TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, portalTrace );
				pPortalPlayer->m_hPortalEnvironment = NULL;

				if( !portalTrace.startsolid ) //if we startsolid in the portal environment, leave the position alone for now on the assumption that it might be exploitable otherwise
				{
					//fix it
					CTraceFilterSimple traceFilter( player, COLLISION_GROUP_PLAYER_MOVEMENT );
					
					Vector vResult = vec3_origin;
					Vector vExtents = (pPortalPlayer->GetHullMaxs() - pPortalPlayer->GetHullMins()) * 0.5f;
					UTIL_FindClosestPassableSpace( mv->GetAbsOrigin() + vOriginToCenter, vExtents, Vector( 0.0f, 0.0f, 1.0f ), &traceFilter, MASK_PLAYERSOLID, 100, vResult );
					mv->SetAbsOrigin( vResult - vOriginToCenter );
				}
			}
		}
		
	}
	
#if defined( TRACE_DEBUG_ENABLED )
	CheckStuck();
#endif

#if defined( GAME_DLL ) && 0
	if( pPortalEnvironment )
	{
		bool bShowBox = true;
#if 1 //only draw the box if we're tracing the other side
		Ray_t ray;
		ray.Init( mv->GetAbsOrigin(), mv->GetAbsOrigin(), GetPlayerMins(), GetPlayerMaxs() );

		bShowBox = pPortalEnvironment->m_PortalSimulator.IsRayInPortalHole( ray ) == RIPHR_TOUCHING_HOLE_NOT_WALL;
#endif

		if( bShowBox )
		{
			Vector vExtent_WH( 0.0f, pPortalEnvironment->GetHalfWidth(), pPortalEnvironment->GetHalfHeight() );
			NDebugOverlay::BoxAngles( pPortalEnvironment->GetAbsOrigin(), -vExtent_WH, vExtent_WH + Vector( 2.0f, 0.0f, 0.0f ), pPortalEnvironment->GetAbsAngles(), 255, 0, 0, 100, 0.0f );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ducked - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CPortalGameMovement::GetPlayerMins( bool ducked ) const
{
	return ducked ? GetPortalPlayer()->GetDuckHullMins() : GetPortalPlayer()->GetStandHullMins();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ducked - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CPortalGameMovement::GetPlayerMaxs( bool ducked ) const
{	
	return ducked ? GetPortalPlayer()->GetDuckHullMaxs() : GetPortalPlayer()->GetStandHullMaxs();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CPortalGameMovement::GetPlayerMins() const
{
	return GetPlayerMins( GetPortalPlayer()->m_Local.m_bDucked );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CPortalGameMovement::GetPlayerMaxs() const
{	
	return GetPlayerMaxs( GetPortalPlayer()->m_Local.m_bDucked );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ducked - 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector& CPortalGameMovement::GetPlayerViewOffset( bool ducked ) const
{
	return ducked ? VEC_DUCK_VIEW : VEC_VIEW;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::CategorizeGroundSurface( trace_t &pm )
{
	IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();
	player->m_surfaceProps = pm.surface.surfaceProps;
	player->m_pSurfaceData = physprops->GetSurfaceData( player->m_surfaceProps );
	physprops->GetPhysicsProperties( player->m_surfaceProps, NULL, NULL, &player->m_surfaceFriction, NULL );

	// HACKHACK: Scale this to fudge the relationship between vphysics friction values and player friction values.
	// A value of 0.8f feels pretty normal for vphysics, whereas 1.0f is normal for players.
	// This scaling trivially makes them equivalent.  REVISIT if this affects low friction surfaces too much.
	player->m_surfaceFriction *= 1.25f;
	if ( player->m_surfaceFriction > 1.0f )
		player->m_surfaceFriction = 1.0f;

	player->m_chTextureType = player->m_pSurfaceData->game.material;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::CheckParameters()
{
	QAngle	v_angle;

	if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
		player->GetMoveType() != MOVETYPE_NOCLIP &&
		player->GetMoveType() != MOVETYPE_OBSERVER )
	{
		float maxspeed;

		bool bIsOnSpeedPaint = mv->m_flMaxSpeed > sv_speed_normal.GetFloat();

		maxspeed = mv->m_flClientMaxSpeed;
		if ( maxspeed != 0.0 )
		{
			mv->m_flMaxSpeed = MIN( maxspeed, mv->m_flMaxSpeed );
		}

		// Slow down by the speed factor
		float flSpeedFactor = 1.0f;
		if (player->m_pSurfaceData)
		{
			flSpeedFactor = player->m_pSurfaceData->game.maxSpeedFactor;
		}

		// If we have a constraint, slow down because of that too.
		float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
			flSpeedFactor = flConstraintSpeedFactor;

		mv->m_flMaxSpeed *= flSpeedFactor;

		float flSideMoveFactor = 1.0f;
		float flForwardMoveFactor = 1.0f;

		//If the player is on speed paint and pressing both a forward/backward and left/right movement keys
		if( bIsOnSpeedPaint && player->GetGroundEntity() &&
			mv->m_flForwardMove != 0.f && mv->m_flSideMove != 0.f )
		{
			const float flForwardSpeed = fabs( DotProduct( player->Forward(), mv->m_vecVelocity ) );
			const float flSideSpeed = fabs( DotProduct( player->Left(), mv->m_vecVelocity ) );

			// Figure out which direction we're more moving in: Side to side or forward/backward. then dampen our
			// input in the direction we're not mostly traveling
			if( flForwardSpeed > flSideSpeed )
			{
				flSideMoveFactor = sv_speed_paint_side_move_factor.GetFloat();
			}
			else
			{
				flForwardMoveFactor = sv_speed_paint_side_move_factor.GetFloat();
			}
		}

		// Go faster on speed paint
		if( bIsOnSpeedPaint )
		{
			float flSpeedPaintMultiplier = mv->m_flMaxSpeed / sv_speed_normal.GetFloat();
			mv->m_flForwardMove *= flSpeedPaintMultiplier;
			mv->m_flSideMove    *= flSpeedPaintMultiplier;
			mv->m_flUpMove      *= flSpeedPaintMultiplier; // do we need this?
		}

		float spdsqr = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
					( mv->m_flSideMove * mv->m_flSideMove ) +
					( mv->m_flUpMove * mv->m_flUpMove );
		
		// Cap off the movement speed, if necessary
		if ( spdsqr != 0.0 )
		{
			float flMax = bIsOnSpeedPaint ? mv->m_flMaxSpeed : sv_speed_normal.GetFloat();
			if ( spdsqr > flMax*flMax )
			{
				float fRatio = mv->m_flMaxSpeed / sqrt( spdsqr );
				mv->m_flForwardMove *= fRatio * flForwardMoveFactor;
				mv->m_flSideMove    *= fRatio * flSideMoveFactor;
				mv->m_flUpMove      *= fRatio;
			}
		}
	}

	if ( player->GetFlags() & FL_FROZEN ||
		player->GetFlags() & FL_ONTRAIN || 
		IsDead() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove    = 0;
		mv->m_flUpMove      = 0;
	}

	DecayPunchAngle();

	// Take angles from command.
	if ( !IsDead() )
	{
		v_angle = mv->m_vecAngles;
		v_angle = v_angle + player->m_Local.m_vecPunchAngle;

		// Now adjust roll angle
		if ( player->GetMoveType() != MOVETYPE_ISOMETRIC  &&
			player->GetMoveType() != MOVETYPE_NOCLIP && 
			sv_rollangle.GetFloat() != 0 )
		{
			mv->m_vecAngles[ROLL]  = CalcRoll( v_angle, mv->m_vecVelocity, sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
		}
		else
		{
			mv->m_vecAngles[ROLL] = 0.0; // v_angle[ ROLL ];
		}
		mv->m_vecAngles[PITCH] = v_angle[PITCH];
		mv->m_vecAngles[YAW]   = v_angle[YAW];
	}
	else
	{
		mv->m_vecAngles = mv->m_vecOldAngles;
	}

	// Set dead player view_offset
	if ( IsDead() )
	{
		player->SetViewOffset( VEC_DEAD_VIEWHEIGHT );
	}

	// Adjust client view angles to match values used on server.
	if ( mv->m_vecAngles[YAW] > 180.0f )
	{
		mv->m_vecAngles[YAW] -= 360.0f;
	}
}


// get a conservative bounds for this player's movement traces
// This allows gamemovement to optimize those traces
void CPortalGameMovement::SetupMovementBounds( CMoveData *move )
{
	if ( m_pTraceListData )
	{
		m_pTraceListData->Reset();
	}
	else
	{
		m_pTraceListData = enginetrace->AllocTraceListData();
	}
	if ( !move->m_nPlayerHandle.IsValid() )
	{
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)move->m_nPlayerHandle.Get();
	CPortal_Player *pPortalPlayer = assert_cast< CPortal_Player* >( pPlayer );

	Vector moveMins, moveMaxs;
	ClearBounds( moveMins, moveMaxs );
	Vector start = move->GetAbsOrigin();
	float radius = ((move->m_vecVelocity.Length() + move->m_flMaxSpeed) * gpGlobals->frametime) + 1.0f;
	// NOTE: assumes the unducked bbox encloses the ducked bbox
	Vector boxMins = pPortalPlayer->GetStandHullMins(); //GetPlayerMins(false);
	Vector boxMaxs = pPortalPlayer->GetStandHullMaxs(); //GetPlayerMaxs(false);

	// bloat by traveling the max velocity in all directions, plus the stepsize up/down
	Vector bloat;
	bloat.Init(radius, radius, radius);
	bloat += pPlayer->m_Local.m_flStepSize * pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal;
	AddPointToBounds( start + boxMaxs + bloat, moveMins, moveMaxs );
	AddPointToBounds( start + boxMins - bloat, moveMins, moveMaxs );
	// now build an optimized trace within these bounds
	enginetrace->SetupLeafAndEntityListBox( moveMins, moveMaxs, m_pTraceListData );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::StartGravity()
{
	float ent_gravity;

	if (player->GetGravity())
		ent_gravity = player->GetGravity();
	else
		ent_gravity = 1.0;

#if defined( DEBUG_MOTION_CONTROLLERS )
	DiffPrint( "CPGM:SG %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f", ent_gravity, sv_gravity.GetFloat(), gpGlobals->frametime, XYZ( m_vGravityDirection ), XYZ( player->GetBaseVelocity() )  );
#endif

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.  
	mv->m_vecVelocity += m_vGravityDirection * ( ent_gravity * sv_gravity.GetFloat() * 0.5 * gpGlobals->frametime );
	mv->m_vecVelocity += m_vGravityDirection * ( DotProduct( player->GetBaseVelocity(), m_vGravityDirection ) * gpGlobals->frametime );

	Vector temp = player->GetBaseVelocity();
	temp -= m_vGravityDirection * DotProduct( player->GetBaseVelocity(), m_vGravityDirection );
	player->SetBaseVelocity( temp );

	CheckVelocity();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::AddGravity()
{
	if ( player->m_flWaterJumpTime == 0 )
	{
		const float ent_gravity = (player->GetGravity() != 0) ? player->GetGravity() : 1.0f;

		// Add gravity incorrectly
		mv->m_vecVelocity += (ent_gravity * sv_gravity.GetFloat() * gpGlobals->frametime) * m_vGravityDirection;
		mv->m_vecVelocity -= DotProduct( player->GetBaseVelocity(), m_vGravityDirection ) * gpGlobals->frametime * m_vGravityDirection;
		Vector temp = player->GetBaseVelocity();
		temp -= DotProduct( temp, m_vGravityDirection ) * m_vGravityDirection;
		player->SetBaseVelocity( temp );

		CheckVelocity();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::FinishGravity()
{
	if ( player->m_flWaterJumpTime == 0 )
	{
		const float ent_gravity = (player->GetGravity() != 0) ? player->GetGravity() : 1.0f;

		// Get the correct velocity for the end of the dt 
		mv->m_vecVelocity += (ent_gravity * sv_gravity.GetFloat() * gpGlobals->frametime * 0.5) * m_vGravityDirection;

		CheckVelocity();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Does the basic move attempting to climb up step heights.  It uses
//          the mv->GetAbsOrigin() and mv->m_vecVelocity.  It returns a new
//          new mv->GetAbsOrigin(), mv->m_vecVelocity, and mv->m_outStepHeight.
//-----------------------------------------------------------------------------
void CPortalGameMovement::StepMove( Vector &vecDestination, trace_t &traceIn )
{
	CTrace_PlayerAABB_vs_Portals trace;
	*(trace_t *)&trace = traceIn;
	trace.m_bContactedPortalTransitionRamp = false;

	// Save the move position and velocity in case we need to put it back later.
	Vector vecPos = mv->GetAbsOrigin(),
		vecVel = mv->m_vecVelocity;

	// First try walking straight to where they want to go.
	Vector vecEndPos = vecDestination;
	TryPlayerMove( &vecEndPos, &trace );

	// mv now contains where they ended up if they tried to walk straight there.
	// Save those results for use later.
	Vector vecDownPos = mv->GetAbsOrigin(),
		vecDownVel = mv->m_vecVelocity;

	// Reset original values to try some other things.
	mv->SetAbsOrigin( vecPos );
	mv->m_vecVelocity = vecVel;

	// Move up a stair height.
	// Slide forward at the same velocity but from the higher position.
	vecEndPos = mv->GetAbsOrigin();
	if ( player->m_Local.m_bAllowAutoMovement )
	{
		vecEndPos -= (player->m_Local.m_flStepSize + DIST_EPSILON) * m_vGravityDirection;
	}

	// Only step up as high as we have headroom to do so.	
	TracePlayerBBox( mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( !trace.startsolid && !trace.allsolid )
	{
		mv->SetAbsOrigin( trace.endpos );
	}
	TryPlayerMove( 0, 0 );

	// Move down a stair (attempt to).
	// Slide forward at the same velocity from the lower position.
	vecEndPos = mv->GetAbsOrigin();
	if ( player->m_Local.m_bAllowAutoMovement )
	{
		vecEndPos += (player->m_Local.m_flStepSize + DIST_EPSILON) * m_vGravityDirection;
	}

#if defined GAME_DLL
	RANDOM_CEG_TEST_SECRET();
#endif

	TracePlayerBBox( mv->GetAbsOrigin(), vecEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	// If we are not on the ground any more then use the original movement attempt.
	// Note: Negation of dot product here because gravity direction is the negative normal direction
	bool bOnGround = true;
	if ( (-DotProduct( trace.plane.normal, m_vGravityDirection ) < CRITICAL_SLOPE) && !trace.HitPortalRamp(-m_vGravityDirection) )
	{
		mv->SetAbsOrigin( vecDownPos );
		VectorCopy( vecDownVel, mv->m_vecVelocity );
		bOnGround = false;
	}

	// If still on the ground
	if( bOnGround )
	{
		// If the trace ended up in empty space, copy the end over to the origin.
		if ( !trace.startsolid && !trace.allsolid )
		{
			mv->SetAbsOrigin( trace.endpos );
		}

		// Copy this origin to up.
		Vector vecUpPos = mv->GetAbsOrigin();

		// decide which one went farther
		float flDownDistSqr = ( vecDownPos - vecPos ).LengthSqr();
		float flUpDistSqr = ( vecUpPos - vecPos ).LengthSqr();
		if ( flDownDistSqr > flUpDistSqr )
		{
			mv->SetAbsOrigin( vecDownPos );
			mv->m_vecVelocity = vecDownVel;
		}
		else 
		{
			// copy z value from slide move
			mv->m_vecVelocity -= DotProduct( mv->m_vecVelocity, m_vGravityDirection );
			mv->m_vecVelocity += DotProduct( vecDownVel, m_vGravityDirection ) * m_vGravityDirection;
		}
	}

	// Compute step distance
	float flStepDist = DotProduct( m_vGravityDirection, vecPos - mv->GetAbsOrigin() );
	if ( flStepDist > 0 )
	{
		mv->m_outStepHeight += flStepDist;
	}

	traceIn = *(trace_t *)&trace;
}

void CPortalGameMovement::CheckWallImpact( Vector& primal_velocity )
{
	// Check if they slammed into a wall
	float fSlamVol = 0.0f;

	Vector vPrimalVelInPlane = primal_velocity - m_vGravityDirection * DotProduct( primal_velocity, m_vGravityDirection );
	Vector vVelInPlane = mv->m_vecVelocity - m_vGravityDirection * DotProduct( mv->m_vecVelocity, m_vGravityDirection );

	float fLateralStoppingAmount = vPrimalVelInPlane.Length() - vVelInPlane.Length();
	if ( fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED )
	{
#if defined CLIENT_DLL
		STEAMWORKS_TESTSECRET();
#endif
		fSlamVol = 1.0f;
	}
	else if ( fLateralStoppingAmount > 0.5f * PLAYER_MAX_SAFE_FALL_SPEED )
	{
		fSlamVol = 0.85f;
	}

	if ( fSlamVol > 0.0f )
	{
		PlayerWallImpactEffects( fSlamVol, DotProduct( (vPrimalVelInPlane - vVelInPlane).Normalized(), vPrimalVelInPlane) );
	}

	// Check if the player slammed into a ceiling
	fSlamVol = 0.0f;
	float flLostVerticalSpeed = primal_velocity.z - mv->m_vecVelocity.z;
	if( flLostVerticalSpeed > PLAYER_MAX_SAFE_FALL_SPEED )
	{
		fSlamVol = 1.0f;
	}
	else if( flLostVerticalSpeed > 0.5f * PLAYER_MAX_SAFE_FALL_SPEED )
	{
		fSlamVol = 0.85f;
	}

	if( fSlamVol > 0.0f )
	{
		PlayerCeilingImpactEffects( fSlamVol );
	}

	// Note: The check for a rough landing on the floor is in CheckFalling().

	// impact animation
	if ( g_pGameRules->IsMultiplayer() )
	{
		const float flImpactThreshold = coop_impact_velocity_threshold.GetFloat();
		float flLostHorizontalSpeed = primal_velocity.Length2D() - mv->m_vecVelocity.Length2D();
		Activity impactActivity = ACT_INVALID;
		if ( flLostVerticalSpeed > flImpactThreshold )
		{
			impactActivity = ACT_MP_JUMP_IMPACT_TOP;
		}
		else if ( flLostHorizontalSpeed > flImpactThreshold )
		{
			const float flDot45Degree = 0.707106781187;
			Vector vImpactDir = primal_velocity - mv->m_vecVelocity;
			vImpactDir.NormalizeInPlace();
			float flImpactDot = DotProduct( vImpactDir, player->Forward() );
			float flOrthoDot = DotProduct( vImpactDir, -player->Left() );

			// Do nothing at the moment
			if ( flImpactDot < -flDot45Degree )
			{
				// Back impact
				impactActivity = ACT_MP_JUMP_IMPACT_S;
			}
			else if ( flImpactDot > flDot45Degree )
			{
				// Head-on impact
				impactActivity = ACT_MP_JUMP_IMPACT_N;
			}
			else if ( flOrthoDot > 0.0f )
			{
				// Right impact
				impactActivity = ACT_MP_JUMP_IMPACT_E;
			}
			else
			{
				// Left impact
				impactActivity = ACT_MP_JUMP_IMPACT_W;
			}
		}

		if ( impactActivity != ACT_INVALID )
		{
			CPortal_Player *pPortalPlayer = ToPortalPlayer( player );
			pPortalPlayer->DoAnimationEvent( PLAYERANIMEVENT_CUSTOM_GESTURE, impactActivity );

			CRecipientFilter filter;
			filter.UsePredictionRules();
			filter.AddRecipientsByPAS( pPortalPlayer->GetAbsOrigin() );
			pPortalPlayer->EmitSound( filter, pPortalPlayer->entindex(), "CoopBot.WallSlam" ); 
		}
	}
}



// Edge friction convars
ConVar sv_edgefriction( "sv_edgefriction", "2", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar sv_use_edgefriction( "sv_use_edgefriction", "1", FCVAR_CHEAT | FCVAR_REPLICATED );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::Friction()
{
	float	speed, newspeed, control;
	float	friction;
	float	drop;

	// If we are in water jump cycle, don't apply friction
	if (player->m_flWaterJumpTime)
		return;

	// Calculate speed
	speed = mv->m_vecVelocity.Length();

	// If too slow, return
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

	// apply ground friction
	if ( player->GetGroundEntity() != NULL )  // On an entity that is the ground
	{
		// Set our base friction for the surface
		friction = sv_friction.GetFloat() * player->m_surfaceFriction;

		if ( sv_use_edgefriction.GetBool() )
		{
			Vector start, stop;
			trace_t pm;

			// NOTE: added a "1.0f" to the player minimum (bbox) value so that the 
			//       trace starts just inside of the bounding box, this make sure
			//       that we don't get any collision epsilon (on surface) errors.
			//		 The significance of the 16 below is this is how many units out front we are checking
			//		 to see if the player box would fall.  The 49 is the number of units down that is required
			//		 to be considered a fall.  49 is derived from 1 (added 1 from above) + 48 the max fall 
			//		 distance a player can fall and still jump back up.
			//
			//		 UNDONE: In some cases there are still problems here.  Specifically, no collision check is
			//		 done so 16 units in front of the player could be inside a volume or past a collision point.

			Vector vOrigin = player->GetAbsOrigin();
			//start[0] = stop[0] = vOrigin[0] + (mv->m_vecVelocity[0]/speed)*16;
			//start[1] = stop[1] = vOrigin[1] + (mv->m_vecVelocity[1]/speed)*16;
			//start[2] = vOrigin[2] + ( GetPlayerMins()[2] + 1.0f );
			//stop[2] = start[2] - 49;
			Vector directionInPlane = mv->m_vecVelocity;
			directionInPlane -= DotProduct( directionInPlane, m_vGravityDirection ) * directionInPlane;
			directionInPlane.NormalizeInPlace();
			start = vOrigin + 16.0f * directionInPlane;	// Offset 16 units in the travel direction
			start -= m_vGravityDirection;				// Bump up 1 unit
			stop = start + 49.0f * m_vGravityDirection;	// Trace down 49 units

			// This should *not* be a raytrace, it should be a box trace such that we can determine if the
			// player's box would fall off the ledge.  Ray tracing has problems associated with walking on rails
			// or on planks where a single ray would have the code believe the player is going to fall when in fact
			// they wouldn't.  The by product of this not working properly is that when a player gets to what
			// the code believes is an edge, the friction is bumped way up thus slowing the player down.
			// If not done properly, this kicks in way too often and forces big unintentional slowdowns.

			UTIL_TraceHull( start, stop, GetPlayerMins(), GetPlayerMaxs(), MASK_PLAYERSOLID, player, COLLISION_GROUP_PLAYER_MOVEMENT, &pm );

			/*
			NDebugOverlay::Cross( start, 8.0f, 255, 0, 0, true, 2.0f );
			NDebugOverlay::Cross( stop, 8.0f, 0, 255, 0, true, 2.0f );
			*/

			// If we didn't contact the ground, we need to apply some friction!
			if ( pm.fraction == 1.0 )
			{
				friction *= sv_edgefriction.GetFloat();
				//NDebugOverlay::SweptBox( start, stop, GetPlayerMins(), GetPlayerMaxs(), QAngle(0,0,0), 0, 255, 0, 32, 5 );
			}
			//else
			//{
			//	NDebugOverlay::SweptBox( start, stop, GetPlayerMins(), GetPlayerMaxs(), QAngle(0,0,0), 255, 0, 0, 32, 5 );
			//}
		}

		// Bleed off some speed, but if we have less than the bleed
		//  threshold, bleed the threshold amount.

		if ( IsCrossPlayPlatformAConsole( player->GetCrossPlayPlatform() ) )
		{
			if( player->m_Local.m_bDucked )
			{
				control = (speed < sv_stopspeed.GetFloat()) ? sv_stopspeed.GetFloat() : speed;
			}
			else
			{
#if defined ( TF_DLL ) || defined ( TF_CLIENT_DLL )
				control = (speed < sv_stopspeed.GetFloat()) ? sv_stopspeed.GetFloat() : speed;
#else
				control = (speed < sv_stopspeed.GetFloat()) ? (sv_stopspeed.GetFloat() * 2.0f) : speed;
#endif
			}
		}
		else
		{
			control = (speed < sv_stopspeed.GetFloat()) ? sv_stopspeed.GetFloat() : speed;
		}

		// Add the amount to the drop amount.
		drop += control*friction*gpGlobals->frametime;
	}

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	if ( newspeed != speed )
	{
		// Determine proportion of old speed we are using.
		newspeed /= speed;
		// Adjust velocity according to proportion.
		VectorScale( mv->m_vecVelocity, newspeed, mv->m_vecVelocity );
	}

	mv->m_outWishVel -= (1.f-newspeed) * mv->m_vecVelocity;
}


//-----------------------------------------------------------------------------
// Purpose: Try to keep a walking player on the ground or whatever surface
//			he's stuck to.
//-----------------------------------------------------------------------------
void CPortalGameMovement::StayOnGround()
{
	CTrace_PlayerAABB_vs_Portals trace;
	const Vector& surfaceNormal = GetPortalPlayer()->GetPortalPlayerLocalData().m_StickNormal;
	Vector start = mv->GetAbsOrigin() + surfaceNormal;
	Vector end = mv->GetAbsOrigin() + player->GetStepSize() * m_vGravityDirection;

	// See how far up we can go without getting stuck
	TracePlayerBBox( mv->GetAbsOrigin(), start, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	start = trace.endpos;

	// using trace.startsolid is unreliable here, it doesn't get set when
	// tracing bounding box vs. terrain

	// Now trace down from a known safe position
	TracePlayerBBox( start, end, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	if ( trace.fraction > 0.0f &&			// must go somewhere
		trace.fraction < 1.0f &&			// must hit something
		!trace.startsolid &&				// can't be embedded in a solid
		((DotProduct( trace.plane.normal, surfaceNormal ) >= CRITICAL_SLOPE) || trace.HitPortalRamp(surfaceNormal)) )		// can't hit a steep slope that we can't stand on anyway
	{
		const float flDelta = DotProduct( trace.endpos - mv->GetAbsOrigin(), surfaceNormal );

		// Set the implicit vertical speed keeping the player on the surface
		// Ignore this one if it's on an angled surface, the player is moving, and we get a zero.
		// The values cycle back to zero occasionally while moving on sloped surfaces, which doesn't accurately reflect this implicit speed.
		if( gpGlobals->frametime != 0.0f && ( flDelta != 0.0f || AlmostEqual( trace.plane.normal.z, 1.0f ) || ( mv->m_flSideMove == 0.0f && mv->m_flForwardMove == 0.0f ) ) )
			GetPortalPlayer()->SetImplicitVerticalStepSpeed( flDelta / gpGlobals->frametime );

		// This is incredibly hacky. The real problem is that trace returning that strange value we can't network over.
		if ( fabs( flDelta ) > 0.5f * COORD_RESOLUTION )
		{
			mv->SetAbsOrigin( trace.endpos );
		}
	}
}


void CPortalGameMovement::WaterMove()
{
	int		i;
	Vector	wishvel;
	float	wishspeed;
	Vector	wishdir;
	Vector	start, dest;
	Vector  temp;
	trace_t	pm;
	float speed, newspeed, addspeed, accelspeed;
	Vector forward, right, up;

	AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

	//
	// user intentions
	//
	for (i=0 ; i<3 ; i++)
	{
		wishvel[i] = forward[i]*mv->m_flForwardMove + right[i]*mv->m_flSideMove;
	}

	if ( GameRules()->IsMultiplayer() )
	{
		float flSinkSpeed;
		switch ( player->GetTeamNumber() )
		{
		case TEAM_RED:
			flSinkSpeed = eggbot_sink_speed.GetFloat();
			break;
		case TEAM_BLUE:
			flSinkSpeed = ballbot_sink_speed.GetFloat();
			break;
		default:
			flSinkSpeed = 60.f;
		}

		wishvel = Vector( 0.2f * wishvel[0], 0.2f * wishvel[1], -flSinkSpeed );
		// slow down the velocity
		mv->m_vecVelocity *= ExponentialDecay( coop_sink_speed_decay.GetFloat(), gpGlobals->frametime );
	}
	else
	{
		// if we have the jump key down, move us up as well
		if (mv->m_nButtons & IN_JUMP)
		{
			wishvel[2] += mv->m_flClientMaxSpeed;
		}
		// Sinking after no other movement occurs
		else if (!mv->m_flForwardMove && !mv->m_flSideMove && !mv->m_flUpMove)
		{
			wishvel[2] -= 60;		// drift towards bottom
		}
		else  // Go straight up by upmove amount.
		{
			// exaggerate upward movement along forward as well
			float upwardMovememnt = mv->m_flForwardMove * forward.z * 2;
			upwardMovememnt = clamp( upwardMovememnt, 0, mv->m_flClientMaxSpeed );
			wishvel[2] += mv->m_flUpMove + upwardMovememnt;
		}
	}

	// Copy it over and determine speed
	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Cap speed.
	if (wishspeed > mv->m_flMaxSpeed)
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Slow us down a bit.
	wishspeed *= 0.8;

	// Water friction
	VectorCopy(mv->m_vecVelocity, temp);
	speed = VectorNormalize(temp);
	if (speed)
	{
		newspeed = speed - gpGlobals->frametime * speed * sv_friction.GetFloat() * player->m_surfaceFriction;
		if (newspeed < 0.1f)
		{
			newspeed = 0;
		}

		VectorScale (mv->m_vecVelocity, newspeed/speed, mv->m_vecVelocity);
	}
	else
	{
		newspeed = 0;
	}

	// water acceleration
	if (wishspeed >= 0.1f)  // old !
	{
		addspeed = wishspeed - newspeed;
		if (addspeed > 0)
		{
			VectorNormalize(wishvel);
			accelspeed = sv_accelerate.GetFloat() * wishspeed * gpGlobals->frametime * player->m_surfaceFriction;
			if (accelspeed > addspeed)
			{
				accelspeed = addspeed;
			}

			for (i = 0; i < 3; i++)
			{
				float deltaSpeed = accelspeed * wishvel[i];
				mv->m_vecVelocity[i] += deltaSpeed;
				mv->m_outWishVel[i] += deltaSpeed;
			}
		}
	}

	VectorAdd (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity);

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	VectorMA (mv->GetAbsOrigin(), gpGlobals->frametime, mv->m_vecVelocity, dest);

	TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );
	if ( pm.fraction == 1.0f )
	{
		VectorCopy( dest, start );
		if ( player->m_Local.m_bAllowAutoMovement )
		{
			start[2] += player->m_Local.m_flStepSize + 1;
		}

		TracePlayerBBox( start, dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

		if (!pm.startsolid && !pm.allsolid)
		{	
			float stepDist = pm.endpos.z - mv->GetAbsOrigin().z;
			mv->m_outStepHeight += stepDist;
			// walked up the step, so just keep result and exit
			mv->SetAbsOrigin( pm.endpos );
			VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
			return;
		}

		// Try moving straight along out normal path.
		TryPlayerMove();
	}
	else
	{
		if ( !player->GetGroundEntity() )
		{
			TryPlayerMove();
			VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
			return;
		}

		StepMove( dest, pm );
	}

	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::WalkMove()
{
	Vector wishvel;
	float spd;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	Vector dest;
	trace_t pm;
	Vector forward, right, up;

	AngleVectors (mv->m_vecViewAngles, &forward, &right, &up);  // Determine movement angles

	CBaseEntity *pOldGround = player->GetGroundEntity();

	// Copy movement amounts
	fmove = mv->m_flForwardMove;
	smove = mv->m_flSideMove;

	// Keep movement vectors in our plane of movement
	forward -= m_vGravityDirection * DotProduct( forward, m_vGravityDirection );
	VectorNormalize( forward );

	right -= m_vGravityDirection * DotProduct( right, m_vGravityDirection );
	VectorNormalize( right );

	// Determine velocity
	wishvel = fmove * forward + smove * right;

	// Don't let them stand on the edge of vertical bridges
	bool shouldShovePlayer = false;
	float wishVelShoveDampenFactor = 0;
	Vector shoveVector = vec3_origin;
	CProjectedWallEntity *pProjectedWall = dynamic_cast< CProjectedWallEntity* >( pOldGround );
	if ( pProjectedWall )
	{
		Vector vBridgeUp = pProjectedWall->Up();
		if ( vBridgeUp.z > -0.4f && vBridgeUp.z < 0.4f )
		{
			wishVelShoveDampenFactor = 0.25f;	// Weaken their manual control
			shoveVector = vBridgeUp * 150.f;	// Shove!
			shouldShovePlayer = true;
		}
	}

	// Don't let players stand on top of each other
	if( pOldGround && pOldGround->IsPlayer() )
	{
		wishVelShoveDampenFactor = 0.25f;
		shoveVector = player->GetAbsOrigin() - pOldGround->GetAbsOrigin();
		shoveVector.z = 0.0f;
		if( shoveVector.IsZeroFast() )
			shoveVector = player->Forward();

		shoveVector.NormalizeInPlace();
		shoveVector *= 150.0f;
		shouldShovePlayer = true;
	}

	if( shouldShovePlayer )
	{
		wishvel *= wishVelShoveDampenFactor;	// Weaken their manual control
		wishvel += shoveVector;					// Shove!
	}

	// Restrict wishvel to plane of movement
	wishvel -= m_vGravityDirection * DotProduct( wishvel, m_vGravityDirection );

	// Try funnelling
	if( sv_player_funnel_into_portals.GetBool() && speed_funnelling_enabled.GetBool() )
	{
		wishvel += PortalFunnel( wishvel );
	}

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if ((wishspeed != 0.0f) && (wishspeed > mv->m_flMaxSpeed))
	{
		VectorScale (wishvel, mv->m_flMaxSpeed/wishspeed, wishvel);
		wishspeed = mv->m_flMaxSpeed;
	}

	// Set pmove velocity
	Accelerate ( wishdir, wishspeed, sv_accelerate.GetFloat() );

	// Add in any base velocity to the current velocity.
	VectorAdd (mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	spd = VectorLength( mv->m_vecVelocity );

	if ( spd < 1.0f )
	{
		mv->m_vecVelocity.Init();
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// first try just moving to the destination	
	dest = mv->GetAbsOrigin() + ( mv->m_vecVelocity * gpGlobals->frametime );

	// first try moving directly to the next spot
	TracePlayerBBox( mv->GetAbsOrigin(), dest, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );

	// If we made it all the way, then copy trace end as new player position.
	mv->m_outWishVel += wishdir * wishspeed;

	if ( pm.fraction == 1 )
	{
		mv->SetAbsOrigin( pm.endpos );
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

		StayOnGround();
		return;
	}

	// Don't walk up stairs if not on ground.
	if ( pOldGround == NULL && player->GetWaterLevel()  == 0 )
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// If we are jumping out of water, don't do anything more.
	if ( player->m_flWaterJumpTime )         
	{
		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );
		return;
	}

	// See if we collided with a ramp
	if( DotProduct( pm.plane.normal, -m_vGravityDirection ) > CRITICAL_SLOPE )
	{
		// Compute normalized forward direction in tangent plane
		const Vector vWishDirection = mv->m_vecVelocity.Normalized();
		const Vector vTangentRight = CrossProduct( vWishDirection, pm.plane.normal );
		const Vector vNormTangentForward = CrossProduct( pm.plane.normal, vTangentRight ).Normalized();

		// Move up the ramp
		float fSpeed = mv->m_vecVelocity.Length();
		Vector vEndPos = player->GetAbsOrigin() + (mv->m_vecVelocity * pm.fraction + vNormTangentForward * (1.0 - pm.fraction) * fSpeed) * gpGlobals->frametime;
		
		//above code has the distinct possibility of placing the player inside a wall. Not quite sure why it works so well most of the time. Add some error checking
		if( sv_portal_new_player_trace.GetBool() )
		{
			trace_t rampTrace;
			TracePlayerBBox( vEndPos, vEndPos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, rampTrace );

			if( !rampTrace.startsolid )
			{
				mv->SetAbsOrigin( vEndPos );
			}
			else
			{
				StepMove( dest, pm );
			}
		}
		else
		{
			mv->SetAbsOrigin( vEndPos );
		}
	}
	else
	{
		StepMove( dest, pm );
	}

#if defined GAME_DLL
	RANDOM_CEG_TEST_SECRET();
#endif

	// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
	VectorSubtract( mv->m_vecVelocity, player->GetBaseVelocity(), mv->m_vecVelocity );

	StayOnGround();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::FullWalkMove()
{
	if ( !CheckWater() ) 
	{
		StartGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if (player->m_flWaterJumpTime)
	{
		WaterJump();
		TryPlayerMove( 0, 0 );
		// See if we are still in water?
		CheckWater();
		return;
	}

	// If we are swimming in the water, see if we are nudging against a place we can jump up out
	//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
	if ( player->GetWaterLevel() >= WL_Waist ) 
	{
		// don't let player jump off the water in coop
		if ( !GameRules()->IsMultiplayer() )
		{
			if ( player->GetWaterLevel() == WL_Waist )
			{
				CheckWaterJump();
			}

			// If we are falling again, then we must not trying to jump out of water any more.
			if ( DotProduct( mv->m_vecVelocity, m_vGravityDirection) > 0.f && 
				player->m_flWaterJumpTime )
			{
				player->m_flWaterJumpTime = 0;
			}

			// Was jump button pressed?
			if (mv->m_nButtons & IN_JUMP)
			{
				CheckJumpButton();
			}
			else
			{
				mv->m_nOldButtons &= ~IN_JUMP;
			}
		}
		else
		{
			CPortal_Player *pPortalPlayer = static_cast< CPortal_Player* >( player );
			if ( !pPortalPlayer->m_Shared.InCond( PORTAL_COND_DROWNING ) )
			{
				pPortalPlayer->m_Shared.AddCond( PORTAL_COND_DROWNING );
			}
		}

		// Perform regular water movement
		WaterMove();

		// Redetermine position vars
		CategorizePosition();

		// If we are on ground, no downward velocity.
		if ( player->GetGroundEntity() != NULL )
		{
			mv->m_vecVelocity.z = 0;
		}
	}
	else
		// Not fully underwater
	{
		// Was jump button pressed?
		if (mv->m_nButtons & IN_JUMP)
		{
			CheckJumpButton();
		}
		else
		{
			mv->m_nOldButtons &= ~IN_JUMP;
		}

		// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
		//  we don't slow when standing still, relative to the conveyor.
		if (player->GetGroundEntity() != NULL)
		{
			mv->m_vecVelocity -= m_vGravityDirection * DotProduct( mv->m_vecVelocity, m_vGravityDirection );
			player->m_Local.m_flFallVelocity = 0.0f;
			Friction();
		}

		// Make sure velocity is valid.
		CheckVelocity();

		CPortal_Player *pPortalPlayer = static_cast< CPortal_Player* >( player );
		if ( pPortalPlayer->m_PortalLocal.m_hTractorBeam.Get() )
		{
			TBeamMove();
		}
		else
		{
			Vector vecVel = mv->m_vecVelocity;

			if ( player->GetGroundEntity() != NULL )
			{
				WalkMove();
			}
			else
			{
#if defined CLIENT_DLL
				RANDOM_CEG_TEST_SECRET();
#endif
				AirMove();  // Take into account movement when in air.
			}

			CheckWallImpact( vecVel );
		}

		// Set final flags.
		CategorizePosition();

		// Make sure velocity is valid.
		CheckVelocity();

		// Add any remaining gravitational component.
		if ( !CheckWater() )
		{
			FinishGravity();
		}

		// If we are on ground, no downward velocity.
		if ( player->GetGroundEntity() != NULL )
		{
			mv->m_vecVelocity -= m_vGravityDirection * DotProduct( mv->m_vecVelocity, m_vGravityDirection );
		}
		CheckFalling();
	}

	if  ( ( m_nOldWaterLevel == WL_NotInWater && player->GetWaterLevel() != WL_NotInWater ) ||
		( m_nOldWaterLevel != WL_NotInWater && player->GetWaterLevel() == WL_NotInWater ) )
	{
		PlaySwimSound();
#if !defined( CLIENT_DLL )
		player->Splash();
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CPortalGameMovement::TryPlayerMove( Vector *pFirstDest, trace_t *pFirstTrace )
{
	int			bumpcount, numbumps;
	Vector		dir;
	float		d;
	int			numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      new_velocity;
	int			i, j;
	trace_t	pm;
	Vector		end;
	float		time_left, allFraction;
	int			blocked;		

	numbumps  = 4;           // Bump up to four times

	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	VectorCopy (mv->m_vecVelocity, original_velocity);  // Store original velocity
	VectorCopy (mv->m_vecVelocity, primal_velocity);

	allFraction = 0;
	time_left = gpGlobals->frametime;   // Total time for this movement operation.

	new_velocity.Init();

	for (bumpcount=0 ; bumpcount < numbumps; bumpcount++)
	{
		if ( mv->m_vecVelocity.Length() == 0.0 )
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA( mv->GetAbsOrigin(), time_left, mv->m_vecVelocity, end );

		// See if we can make it from origin to end point.
		if ( g_bMovementOptimizations )
		{
			// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
			if ( pFirstDest && ( end == *pFirstDest ) )
			{
				pm = *pFirstTrace;
			}
			else
			{
#if defined( PLAYER_GETTING_STUCK_TESTING )
				trace_t foo;
				TracePlayerBBox( mv->GetAbsOrigin(), mv->GetAbsOrigin(), PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, foo );
				if ( foo.startsolid || foo.fraction != 1.0f )
				{
					Msg( "bah\n" );
				}
#endif
				TracePlayerBBox( mv->GetAbsOrigin(), end, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );
			}
		}
		else
		{
			TracePlayerBBox( mv->GetAbsOrigin(), end, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, pm );
		}

		allFraction += pm.fraction;

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (pm.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, mv->m_vecVelocity);
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if( pm.fraction > 0 )
		{	

			// NOTE: Disabled this in portal as we don't have displacement collisions and all traces are amplified greatly making each TracePlayerBBox() really expensive
#if defined(RECHECK_TERRAIN_COLLISION_BUG)
			if ( numbumps > 0 && pm.fraction == 1 )
			{
				// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
				// when the end position is stuck in the triangle.  Re-run the test with an uswept box to catch that
				// case until the bug is fixed.
				// If we detect getting stuck, don't allow the movement
				trace_t stuck;
				TracePlayerBBox( pm.endpos, pm.endpos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, stuck );
				if ( stuck.startsolid || stuck.fraction != 1.0f )
				{
					//Msg( "Player will become stuck!!!\n" );
					VectorCopy (vec3_origin, mv->m_vecVelocity);
					break;
				}
			}
#endif

#if defined( PLAYER_GETTING_STUCK_TESTING )
			trace_t foo;
			TracePlayerBBox( pm.endpos, pm.endpos, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, foo );
			if ( foo.startsolid || foo.fraction != 1.0f )
			{
				Msg( "Player will become stuck!!!\n" );
			}
#endif
			// actually covered some distance
			mv->SetAbsOrigin( pm.endpos);
			VectorCopy (mv->m_vecVelocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (pm.fraction == 1)
		{
			break;		// moved the entire distance
		}

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		MoveHelper( )->AddToTouched( pm, mv->m_vecVelocity );


		if ( player->GetAbsVelocity().AsVector2D().Length() > 275.0f )
		{
			if ( pm.m_pEnt && pm.m_pEnt->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				// Don't let the player auto grab something that they just ran in to
				// They end up holding it, but with less momentum which is confusing.
				CPortal_Player *pPortalPlayer = static_cast< CPortal_Player* >( player );
				pPortalPlayer->m_flAutoGrabLockOutTime = gpGlobals->curtime;

#ifdef CLIENT_DLL
				STEAMWORKS_TESTSECRET();

				if ( pm.m_pEnt == pPortalPlayer->m_hUseEntToSend.Get() )
				{
					pPortalPlayer->m_hUseEntToSend = NULL;
				}
#endif
			}
		}

		// If the plane we hit has a normal whose dot product with the gravity direction is high
		// then this is probably a floor relative to our orientation
		if ( DotProduct( pm.plane.normal, m_vGravityDirection ) > CRITICAL_SLOPE )
		{
			blocked |= 1;		// floor
		}
		// If the plane's normal dotted with the gravity direction is 0 then it's a wall relative to our orientation
		if ( DotProduct( pm.plane.normal, m_vGravityDirection ) == 0.f )
		{
			blocked |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * pm.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (vec3_origin, mv->m_vecVelocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (pm.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if ( numplanes == 1 &&
			player->GetMoveType() == MOVETYPE_WALK &&
			player->GetGroundEntity() == NULL )	
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( DotProduct( planes[i], m_vGravityDirection ) > CRITICAL_SLOPE  )
				{
					// floor or slope
					ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
				{
					ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + sv_bounce.GetFloat() * (1 - player->m_surfaceFriction) );
				}
			}

			VectorCopy( new_velocity, mv->m_vecVelocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i < numplanes ; i++)
			{
				ClipVelocity( original_velocity, planes[i], mv->m_vecVelocity, 1 );

				for (j=0 ; j<numplanes ; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->m_vecVelocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				}

				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy (vec3_origin, mv->m_vecVelocity);
					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(mv->m_vecVelocity);
				VectorScale (dir, d, mv->m_vecVelocity );
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = mv->m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (vec3_origin, mv->m_vecVelocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (vec3_origin, mv->m_vecVelocity);
	}

	return blocked;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : in - 
//			normal - 
//			out - 
//			overbounce - 
// Output : int
//-----------------------------------------------------------------------------
int CPortalGameMovement::ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce )
{
	const Vector& stickNormal = GetPortalPlayer()->GetPortalPlayerLocalData().m_StickNormal;
	const float angle = DotProduct( normal, stickNormal );

	int blocked = 0x00;     // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 


	// Determine how far along plane to slide based on incoming direction.
	const float backoff = DotProduct( in, normal ) * overbounce;
	out = in - backoff * normal;

	// iterate once to make sure we aren't still moving through the plane
	const float adjust = DotProduct( out, normal );
	if( adjust < 0.0f )
	{
		out -= ( normal * adjust );
		//		Msg( "Adjustment = %lf\n", adjust );
	}

	// Return blocking flags.
	return blocked;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if crouch/uncrouch caused player to get stuck in world
// Input  : direction - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::FixPlayerCrouchStuck( bool upward )
{
	EntityHandle_t hitent;
	int i;
	Vector test;
	trace_t dummy;

	int direction = upward ? 1 : 0;

	hitent = TestPlayerPosition( mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, dummy );
	if (hitent == INVALID_ENTITY_HANDLE )
		return;

	VectorCopy( mv->GetAbsOrigin(), test );	
	for ( i = 0; i < 36; i++ )
	{
		Vector org = mv->GetAbsOrigin();
		org += direction * -m_vGravityDirection;
		mv->SetAbsOrigin( org );
		hitent = TestPlayerPosition( mv->GetAbsOrigin(), COLLISION_GROUP_PLAYER_MOVEMENT, dummy );
		if (hitent == INVALID_ENTITY_HANDLE )
			return;
	}

	mv->SetAbsOrigin( test ); // Failed
}


bool CPortalGameMovement::CanUnduck()
{
	Vector newOrigin = mv->GetAbsOrigin();
	const CPortal_Player* pPortalPlayer = GetPortalPlayer(); 

	if ( player->GetGroundEntity() != NULL )
	{
		//newOrigin += pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetStandHullMins();

		// EXTREMELY USEFUL HACK: When sticking to the ceiling, the bounding box still expands upwards,
		// but we move the player down when this happens, so the trace needs to be different
		//if (onCeiling)
		//{
		//	// Move the origin to the new position and down by an epsilon
		//	newOrigin.z += pPortalPlayer->GetDuckHullMaxs().z - pPortalPlayer->GetStandHullMaxs().z;
		//	newOrigin.z -= EQUAL_EPSILON;
		//}
	}
	else
	{
		// If in air an letting go of crouch, make sure we can offset origin to make
		//  up for uncrouching
		if ( player->m_Local.m_bDucked )
		{
			Vector hullSizeNormal = pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins();
			Vector hullSizeCrouch = pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins();
			Vector originDelta = ( hullSizeCrouch - hullSizeNormal );
			originDelta *= 0.5f; //only move by half delta to maintain center
			newOrigin += originDelta;
		}
	}

	// Don't use the portal trace if the player is in a portal linked to another at a drastic angle.
	// This will prevent us from finding empty space on the far side of the portal and erroneously
	// deciding to uncrouch when we need to crouch to get through the portal.
	bool bUsePortalTrace = true;
	if( pPortalPlayer != NULL )
	{
		// If the player just duck jumped in a portal environment, she probably teleported.
		// Make sure she doesn't screw up a fling.
		const CPortal_Base2D* pPortal = pPortalPlayer->m_hPortalEnvironment.Get();
		if( pPortal )
		{
			//portal facing at least a little bit up, player at a sufficient fling speed, and that velocity is also at least a little bit upward
			if( ( player->GetGroundEntity() == NULL ) && ShouldMaintainFlingAssistCrouch( pPortal, mv->m_vecVelocity ) )
			{
				//fling case, require that whichever extent is closest to the portal plane has cleared it. Otherwise we unduck and catch on edges as we exit
				Vector vOriginToCenter = (pPortalPlayer->GetHullMaxs() + pPortalPlayer->GetHullMins()) * 0.5f;
				//Vector vPortalToCenter = mv->GetAbsOrigin() + vOriginToCenter - pPortal->m_ptOrigin;
				//Vector vOffCenter = vPortalToCenter - (vPortalToCenter.Dot( pPortal->m_vForward ) * pPortal->m_vForward);

				Vector vTestPoint = mv->GetAbsOrigin() + vOriginToCenter;
				Vector vStandExtents = (pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins()) * 0.5f;
				for( int k = 0; k != 3; ++k )
				{
					vTestPoint[k] -= vStandExtents[k] * Sign( pPortal->m_vForward[k] );
					//vTestPoint[k] += vStandExtents[k] * Sign( vOffCenter[k] );
				}

				if( pPortal->m_plane_Origin.normal.Dot( vTestPoint ) < pPortal->m_plane_Origin.dist )
				{
					return false;
				}

#if defined( DEBUG_FLINGS )
				s_MovementDebug.vUnduckPos = mv->GetAbsOrigin() + vOriginToCenter;
				s_MovementDebug.vUnduckVel = mv->m_vecVelocity;
#endif
				//NDebugOverlay::Box( mv->GetAbsOrigin() + vOriginToCenter, -vStandExtents, vStandExtents, 255, 0, 0, 100, 30.0f );
				//NDebugOverlay::Box( vTestPoint, -Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ), 0, 0, 255, 100, 30.0f );
			}

			matrix3x4_t matTransform = pPortal->MatrixThisToLinked().As3x4();
			Vector vWorldUp;
			VectorRotate( Vector( 0.0f, 0.0f, 1.0f ), matTransform, vWorldUp );
			const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians
			const float flUpDot = fabs( vWorldUp.z );

			// There's a significant rotation between the portal orientations, less than 90 degrees.
			// 90 degree rotations don't have the disadvantage of uncrouching too soon, and doing
			// so too late causes bugs when the player intentionally crouch-walks through a portal.
			// Also, the player must be moving through the portal at considerable speed. Otherwise,
			// it doesn't matter much when she uncrouches.
			if( flUpDot < COS_PI_OVER_SIX && flUpDot >= EQUAL_EPSILON && pPortalPlayer->MaxSpeed() > sv_speed_normal.GetFloat() )
			{
				bUsePortalTrace = false;
			}
		}
	}

	const bool saveducked = player->m_Local.m_bDucked;
	player->m_Local.m_bDucked = false;
	trace_t trace;
	if( bUsePortalTrace )
		TracePlayerBBox( mv->GetAbsOrigin(), newOrigin, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	else
		CGameMovement::TracePlayerBBox( mv->GetAbsOrigin(), newOrigin, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	player->m_Local.m_bDucked = saveducked;

	// In this case, if the fraction is 1, the player can move all the way to the new origin.
	if ( trace.startsolid || trace.fraction != 1.0f )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Stop ducking
//-----------------------------------------------------------------------------
void CPortalGameMovement::FinishUnDuck()
{
	trace_t trace;
	Vector newOrigin = mv->GetAbsOrigin();
	CPortal_Player* pPortalPlayer = GetPortalPlayer();

	if ( player->GetGroundEntity() != NULL )
	{
		//newOrigin += pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetStandHullMins();

		// EXTREMELY USEFUL HACK: When sticking to the ceiling, the bounding box still expands upwards,
		// but we move the player down when this happens, so the trace needs to be different
		//if (onCeiling)
		//{
		//	// Move the origin to the new position and down by an epsilon
		//	newOrigin.z += pPortalPlayer->GetDuckHullMaxs().z - pPortalPlayer->GetStandHullMaxs().z;
		//	newOrigin.z -= EQUAL_EPSILON;
		//}
	}
	else
	{
		// If in air an letting go of crouch, make sure we can offset origin to make
		//  up for uncrouching
		if ( player->m_Local.m_bDucked )
		{
			Vector hullSizeNormal = pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins();
			Vector hullSizeCrouch = pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins();
			Vector originDelta = ( hullSizeCrouch - hullSizeNormal );
			originDelta *= 0.5f; //only move by half delta to maintain center
			newOrigin += originDelta;
		}
	}

	pPortalPlayer->SetAirDuck( false );
	pPortalPlayer->UnDuck();

	player->m_Local.m_bDucked = false;
	player->RemoveFlag( FL_DUCKING );
	player->m_Local.m_bDucking  = false;
	player->m_Local.m_bInDuckJump  = false;
	player->SetViewOffset( GetPlayerViewOffset( false ) );
	player->m_Local.m_nDuckTimeMsecs = 0;

	mv->SetAbsOrigin( newOrigin );

	if ( pPortalPlayer )
	{
		pPortalPlayer->SetHullHeight( pPortalPlayer->GetHullHeight() );
	}

	// Recategorize position since ducking can change origin
	CategorizePosition();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CPortalGameMovement::UpdateDuckJumpEyeOffset()
{
	if ( player->m_Local.m_nDuckJumpTimeMsecs != 0 )
	{
		int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckJumpTimeMsecs );
		if ( nDuckMilliseconds > TIME_TO_UNDUCK_MSECS )
		{
			player->m_Local.m_nDuckJumpTimeMsecs = 0;
			SetDuckedEyeOffset( 0.0f );
		}
		else
		{
			float flDuckFraction = SimpleSpline( 1.0f - FractionUnDucked( nDuckMilliseconds ) );
			SetDuckedEyeOffset( flDuckFraction );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortalGameMovement::FinishUnDuckJump( trace_t &trace )
{
	Vector vecNewOrigin;
	VectorCopy( mv->GetAbsOrigin(), vecNewOrigin );

	//  Up for uncrouching.
	/*Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
	Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
	Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );*/
	Vector viewDelta = GetPlayerViewOffset( false ) - GetPlayerViewOffset( true );

	float flDeltaZ = viewDelta.z;
	viewDelta.z *= trace.fraction;
	flDeltaZ -= viewDelta.z;

	player->RemoveFlag( FL_DUCKING );
	player->m_Local.m_bDucked = false;
	player->m_Local.m_bDucking  = false;
	player->m_Local.m_bInDuckJump = false;
	player->m_Local.m_nDuckTimeMsecs = 0;
	player->m_Local.m_nDuckJumpTimeMsecs = 0;
	player->m_Local.m_nJumpTimeMsecs = 0;

	Vector vecViewOffset = GetPlayerViewOffset( false );
	vecViewOffset.z -= flDeltaZ;
	player->SetViewOffset( vecViewOffset );

	VectorSubtract( vecNewOrigin, viewDelta, vecNewOrigin );
	mv->SetAbsOrigin( vecNewOrigin );

	CPortal_Player *pPortalPlayer = ToPortalPlayer( player );
	if ( pPortalPlayer )
	{
		pPortalPlayer->SetHullHeight( pPortalPlayer->GetHullHeight() );
	}

	// Recategorize position since ducking can change origin
	CategorizePosition();
}


//-----------------------------------------------------------------------------
// Purpose: Finish ducking
//-----------------------------------------------------------------------------
void CPortalGameMovement::FinishDuck()
{
	bool bWasDucking = ( player->GetFlags() & FL_DUCKING ) > 0;

	player->AddFlag( FL_DUCKING );
	player->m_Local.m_bDucked = true;
	player->m_Local.m_bDucking = false;

	player->SetViewOffset( GetPlayerViewOffset( true ) );

	CPortal_Player* pPortalPlayer = GetPortalPlayer();

	// HACKHACK - Fudge for collision bug - no time to fix this properly
	if ( player->GetGroundEntity() != NULL )
	{
		//Vector org = mv->GetAbsOrigin();
		//org -= pPortalPlayer->GetDuckHullMins() - pPortalPlayer->GetStandHullMins();

		//mv->SetAbsOrigin( org );
	}
	else
	{
		if ( !bWasDucking )
		{
			Vector hullSizeNormal = pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins();
			Vector hullSizeCrouch = pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins();
			Vector originDelta = ( hullSizeNormal - hullSizeCrouch );
			originDelta *= 0.5f; //only move by half delta to maintain center
			Vector out;
			VectorAdd( mv->GetAbsOrigin(), originDelta, out );
			mv->SetAbsOrigin( out );
		}
	}

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	if ( pPortalPlayer )
	{
		pPortalPlayer->SetHullHeight( pPortalPlayer->GetHullHeight() );
	}

	// Recategorize position since ducking can change origin
	CategorizePosition();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPortalGameMovement::StartUnDuckJump()
{
	player->AddFlag( FL_DUCKING );
	player->m_Local.m_bDucked = true;
	player->m_Local.m_bDucking = false;

	player->SetViewOffset( GetPlayerViewOffset( true ) );

	/*Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
	Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
	Vector viewDelta = ( hullSizeNormal - hullSizeCrouch );*/
	Vector viewDelta = GetPlayerViewOffset( false ) - GetPlayerViewOffset( true );
	Vector out;
	VectorAdd( mv->GetAbsOrigin(), viewDelta, out );
	mv->SetAbsOrigin( out );

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : duckFraction - 
//-----------------------------------------------------------------------------
void CPortalGameMovement::SetDuckedEyeOffset( float duckFraction )
{
	Vector vDuckHullMin = GetPlayerMins( true );
	Vector vStandHullMin = GetPlayerMins( false );

	//float fMore = ( vDuckHullMin.z - vStandHullMin.z );

	//DevMsg( "fMore: %f\n", fMore );

	CPortal_Player *pPortalPlayer = ToPortalPlayer( player );
	bool bOnGround = ( player->GetGroundEntity() != NULL );
	bool bDuckedInAir = pPortalPlayer->GetPortalPlayerLocalData().m_bDuckedInAir;
	bool bDucking = player->m_Local.m_bDucking;
	bool bDucked = player->m_Local.m_bDucked;
	bool bInDuck = ( player->GetFlags() & FL_DUCKING ) ? true : false;
	Vector originDelta = Vector( 0, 0, 0 );

	Vector vecDuckViewOffset = GetPlayerViewOffset( true );
	Vector vecStandViewOffset = GetPlayerViewOffset( false );

	if ( bOnGround )
	{
		/*DevMsg("ground ");
		if ( bDucking )
		{
		DevMsg("ducking\n");
		}
		else
		{
		DevMsg("UNducking\n");
		}*/
	}
	else
	{
		//DevMsg("air ");
		if ( bDucking )
		{
			// started ducking on ground, continued ducking in air
			if ( !bDuckedInAir )
			{
				pPortalPlayer->SetAirDuck( true );
			}
			//DevMsg("ducking\n");
		}
		else
		{
			//DevMsg("UNducking\n");
		}
	}

	if ( bDuckedInAir && ( bInDuck || bDucking ) && !bDucked && !bOnGround ) 
	{
		CPortal_Player *pPortalPlayer = ToPortalPlayer( player );
		Vector hullSizeNormal = pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins();
		Vector hullSizeCrouch = pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins();
		originDelta = ( hullSizeNormal - hullSizeCrouch );
		originDelta *= 0.5f;

		vecDuckViewOffset += originDelta;
	}
	else if ( bDuckedInAir && ( bInDuck || bDucking || bDucked ) && !bOnGround )
	{
		CPortal_Player *pPortalPlayer = ToPortalPlayer( player );
		Vector hullSizeNormal = pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins();
		Vector hullSizeCrouch = pPortalPlayer->GetDuckHullMaxs() - pPortalPlayer->GetDuckHullMins();
		originDelta = ( hullSizeCrouch - hullSizeNormal );
		originDelta *= 0.5f;

		vecStandViewOffset += originDelta;
	}

	//int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	//int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	//int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	//// started ducking in the air, landed and still ducking
	//if ( bDucking && bDuckedInAir && bOnGround )
	//{
	//	vecDuckViewOffset = player->GetViewOffset();
	//}
	/*else if ( bOnGround && bDuckedInAir && buttonsReleased & IN_DUCK )
	{
	vecDuckViewOffset = player->GetViewOffset();
	}*/

	Vector temp = player->GetViewOffset();
	temp.z = ( ( vecDuckViewOffset.z /*- fMore*/ ) * duckFraction ) +
		( vecStandViewOffset.z * ( 1 - duckFraction ) );

	//DevMsg( "temp.z: %f\n", temp.z );

	player->SetViewOffset( temp );
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if we are in a situation where we can unduck jump.
//-----------------------------------------------------------------------------
bool CPortalGameMovement::CanUnDuckJump( trace_t &trace )
{	
	// Don't use the portal trace if the player is in a portal linked to another at a drastic angle.
	// This will prevent us from finding empty space on the far side of the portal and erroneously
	// deciding to uncrouch when we need to crouch to get through the portal.
	bool bUsePortalTrace = true;
	const CPortal_Player* pPortalPlayer = GetPortalPlayer();
	if( pPortalPlayer != NULL )
	{
		// If the player just duck jumped in a portal environment, she probably teleported.
		// Make sure she doesn't screw up a fling.
		const CPortal_Base2D* pPortal = pPortalPlayer->m_hPortalEnvironment.Get();
		if( pPortal )
		{
			//portal facing at least a little bit up, player at a sufficient fling speed, and that velocity is also at least a little bit upward
			if( ShouldMaintainFlingAssistCrouch( pPortal, mv->m_vecVelocity ) )
			{
				//fling case, require that whichever extent is closest to the portal plane has cleared it. Otherwise we unduck and catch on edges as we exit
				Vector vOriginToCenter = (pPortalPlayer->GetHullMaxs() + pPortalPlayer->GetHullMins()) * 0.5f;
				//Vector vPortalToCenter = mv->GetAbsOrigin() + vOriginToCenter - pPortal->m_ptOrigin;
				//Vector vOffCenter = vPortalToCenter - (vPortalToCenter.Dot( pPortal->m_vForward ) * pPortal->m_vForward);

				Vector vTestPoint = mv->GetAbsOrigin() + vOriginToCenter;
				Vector vStandExtents = (pPortalPlayer->GetStandHullMaxs() - pPortalPlayer->GetStandHullMins()) * 0.5f;
				for( int k = 0; k != 3; ++k )
				{
					vTestPoint[k] -= vStandExtents[k] * Sign( pPortal->m_vForward[k] );
					//vTestPoint[k] += vStandExtents[k] * Sign( vOffCenter[k] );
				}

				if( pPortal->m_plane_Origin.normal.Dot( vTestPoint ) < pPortal->m_plane_Origin.dist )
				{
					return false;
				}

#if defined( DEBUG_FLINGS )
				s_MovementDebug.vUnduckJumpPos = mv->GetAbsOrigin() + vOriginToCenter;
				s_MovementDebug.vUnduckJumpVel = mv->m_vecVelocity;
#endif
				//NDebugOverlay::Box( mv->GetAbsOrigin() + vOriginToCenter, -vStandExtents, vStandExtents, 0, 255, 0, 100, 30.0f );
				//NDebugOverlay::Box( vTestPoint, -Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 1.0f ), 0, 0, 255, 100, 30.0f );
			}

			matrix3x4_t matTransform = pPortal->MatrixThisToLinked().As3x4();
			Vector vWorldUp;
			VectorRotate( Vector( 0.0f, 0.0f, 1.0f ), matTransform, vWorldUp );
			const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians
			const float flUpDot = fabs( vWorldUp.z );

			// There's a significant rotation between the portal orientations, less than 90 degrees.
			// 90 degrees rotations don't have the disadvantage of uncrouching too soon, and doing
			// so too late causes bugs when the player intentionally crouch-walks through a portal.
			// Also, the player must be moving through the portal at considerable speed. Otherwise,
			// it doesn't matter much when she uncrouches.
			if( flUpDot < COS_PI_OVER_SIX && flUpDot >= EQUAL_EPSILON )
			{
				//if we unduckjump. That moves our feet down while keeping our top as-is. Instantly moving our center. Which can teleport us back through the portal
				//That's bad. See bugbait #72275
				Vector vOriginDelta = GetPlayerViewOffset( true ) - GetPlayerViewOffset( false );
				if( pPortal->m_plane_Origin.normal.Dot( vOriginDelta ) < 0 ) //if delta would push center more into portal
				{
					Vector vUnduckedCenter = mv->GetAbsOrigin() + vOriginDelta + ((pPortalPlayer->GetStandHullMaxs() + pPortalPlayer->GetStandHullMins()) * 0.5f);
					if( pPortal->m_plane_Origin.normal.Dot( vUnduckedCenter ) < pPortal->m_plane_Origin.dist ) //if unducked origin would be behind portal plane, causing a teleportation
					{
						return false; //block the unduck operation
					}
				}

				if( pPortalPlayer->MaxSpeed() > sv_speed_normal.GetFloat() )
				{
					bUsePortalTrace = false;
				}
			}
		}
	}


	// Trace down to the stand position and see if we can stand.
	Vector vecEnd( mv->GetAbsOrigin() );
	vecEnd.z -= 36.0f;						// This will have to change if bounding hull change!
	if( bUsePortalTrace )
		TracePlayerBBox( mv->GetAbsOrigin(), vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );
	else
		CGameMovement::TracePlayerBBox( mv->GetAbsOrigin(), vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, trace );

	if ( trace.fraction < 1.0f )
	{
		// Find the endpoint.
		vecEnd.z = mv->GetAbsOrigin().z + ( -36.0f * trace.fraction );

		// Test a normal hull.
		trace_t traceUp;
		bool bWasDucked = player->m_Local.m_bDucked;
		player->m_Local.m_bDucked = false;

		if( bUsePortalTrace )
			TracePlayerBBox( vecEnd, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, traceUp );
		else
			CGameMovement::TracePlayerBBox( vecEnd, vecEnd, PlayerSolidMask(), COLLISION_GROUP_PLAYER_MOVEMENT, traceUp );
		player->m_Local.m_bDucked = bWasDucked;

		if ( !traceUp.startsolid )
			return true;	
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CPortalGameMovement::Duck()
{
	int buttonsChanged	= ( mv->m_nOldButtons ^ mv->m_nButtons );	// These buttons have changed this frame
	int buttonsPressed	=  buttonsChanged & mv->m_nButtons;			// The changed ones still down are "pressed"
	int buttonsReleased	=  buttonsChanged & mv->m_nOldButtons;		// The changed ones which were previously down are "released"

	// Check to see if we are in the air.
	bool bInAir = ( player->GetGroundEntity() == NULL );
	bool bInDuck = ( player->GetFlags() & FL_DUCKING ) ? true : false;

	bool bDuckJump = ( player->m_Local.m_nJumpTimeMsecs > 0 );
	bool bDuckJumpTime = ( player->m_Local.m_nDuckJumpTimeMsecs > 0 );

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	// Handle death.
	if ( IsDead() )
		return;

	// Slow down ducked players.
	HandleDuckingSpeedCrop();

	// If the player is holding down the duck button, the player is in duck transition, ducking, or duck-jumping.
	if ( ( mv->m_nButtons & IN_DUCK ) || player->m_Local.m_bDucking  || bInDuck || bDuckJump )
	{
		// DUCK
		if ( ( mv->m_nButtons & IN_DUCK ) || bDuckJump )
		{
			// XBOX SERVER ONLY
#if !defined(CLIENT_DLL)
			if ( IsGameConsole() && buttonsPressed & IN_DUCK )
			{
				// Hinting logic
				if ( player->GetToggledDuckState() && player->m_nNumCrouches < NUM_CROUCH_HINTS )
				{
					UTIL_HudHintText( player, "#Valve_Hint_Crouch" );
					player->m_nNumCrouches++;
				}
			}
#endif
			// Have the duck button pressed, but the player currently isn't in the duck position.
			if ( ( buttonsPressed & IN_DUCK ) && !bInDuck && !bDuckJump && !bDuckJumpTime )
			{
				player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
				player->m_Local.m_bDucking = true;
				// set air ducking state
				{
					CPortal_Player *pPortalPlayer = ToPortalPlayer( player );
					pPortalPlayer->SetAirDuck( bInAir );
				}
			}

			// The player is in duck transition and not duck-jumping.
			if ( player->m_Local.m_bDucking && !bDuckJump && !bDuckJumpTime )
			{
				int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs );

				// Finish in duck transition when transition time is over, in "duck", in air.
				if ( ( nDuckMilliseconds > TIME_TO_DUCK_MSECS ) || bInDuck /*|| bInAir*/ )
				{
					FinishDuck();
				}
				else
				{
					// Calc parametric time
					float flDuckFraction = SimpleSpline( FractionDucked( nDuckMilliseconds ) );
					SetDuckedEyeOffset( flDuckFraction );
				}
			}

			if ( bDuckJump )
			{
				// Make the bounding box small immediately.
				if ( !bInDuck )
				{
					StartUnDuckJump();
				}
				else
				{
					// Check for a crouch override.
					if ( !( mv->m_nButtons & IN_DUCK ) )
					{
						trace_t trace;
						if ( CanUnDuckJump( trace ) )
						{
							FinishUnDuckJump( trace );
							player->m_Local.m_nDuckJumpTimeMsecs = (int)( ( (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS * ( 1.0f - trace.fraction ) ) + (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV );
						}
					}
				}
			}
		}
		// UNDUCK (or attempt to...)
		else
		{
			if ( player->m_Local.m_bInDuckJump )
			{
				// Check for a crouch override.
				if ( !( mv->m_nButtons & IN_DUCK ) )
				{
					trace_t trace;
					if ( CanUnDuckJump( trace ) )
					{
						FinishUnDuckJump( trace );

						if ( trace.fraction < 1.0f )
						{
							player->m_Local.m_nDuckJumpTimeMsecs = (int)( ( (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS * ( 1.0f - trace.fraction ) ) + (float)GAMEMOVEMENT_TIME_TO_UNDUCK_MSECS_INV );
						}
					}
				}
				else
				{
					player->m_Local.m_bInDuckJump = false;
				}
			}

			if ( bDuckJumpTime )
				return;

			// Try to unduck unless automovement is not allowed
			// NOTE: When not onground, you can always unduck
			if ( player->m_Local.m_bAllowAutoMovement || bInAir || player->m_Local.m_bDucking )
			{
				// We released the duck button, we aren't in "duck" and we are not in the air - start unduck transition.
				if ( ( buttonsReleased & IN_DUCK ) )
				{
					if ( bInDuck && !bDuckJump )
					{
						player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
					}
					else if ( player->m_Local.m_bDucking && !player->m_Local.m_bDucked )
					{
						// Invert time if release before fully ducked!!!
						int elapsedMilliseconds = GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs;

						float fracDucked = FractionDucked( elapsedMilliseconds );
						int remainingUnduckMilliseconds = (int)( fracDucked * TIME_TO_UNDUCK_MSECS );

						player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME - TIME_TO_UNDUCK_MSECS + remainingUnduckMilliseconds;
					}
				}


				// Check to see if we are capable of unducking.
				if ( CanUnduck() )
				{
					// or unducking
					if ( ( player->m_Local.m_bDucking || player->m_Local.m_bDucked ) )
					{
						int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs );

						// Finish ducking immediately if duck time is over or not on ground
						if ( nDuckMilliseconds > TIME_TO_UNDUCK_MSECS /*|| ( bInAir && !bDuckJump )*/ )
						{
							FinishUnDuck();
						}
						else
						{
							// Calc parametric time
							float flDuckFraction = SimpleSpline( 1.0f - FractionUnDucked( nDuckMilliseconds ) );
							SetDuckedEyeOffset( flDuckFraction );
							player->m_Local.m_bDucking = true;
						}
					}
				}
				else
				{
					// Still under something where we can't unduck, so make sure we reset this timer so
					//  that we'll unduck once we exit the tunnel, etc.
					if ( player->m_Local.m_nDuckTimeMsecs != GAMEMOVEMENT_DUCK_TIME )
					{
						SetDuckedEyeOffset(1.0f);
						player->m_Local.m_nDuckTimeMsecs = GAMEMOVEMENT_DUCK_TIME;
						player->m_Local.m_bDucked = true;
						player->m_Local.m_bDucking = false;
						player->AddFlag( FL_DUCKING );
					}
					else
					{
						int nDuckMilliseconds = MAX( 0, GAMEMOVEMENT_DUCK_TIME - player->m_Local.m_nDuckTimeMsecs );

						// Finish ducking immediately if duck time is over
						if ( nDuckMilliseconds <= TIME_TO_UNDUCK_MSECS )
						{
							// Calc parametric time
							float flDuckFraction = SimpleSpline( 1.0f - FractionUnDucked( nDuckMilliseconds ) );
							SetDuckedEyeOffset( flDuckFraction );
							player->m_Local.m_bDucking = true;
						}
					}
				}
			}
		}
	}
	// HACK: (jimd 5/25/2006) we have a reoccuring bug (#50063 in Tracker) where the player's
	// view height gets left at the ducked height while the player is standing, but we haven't
	// been  able to repro it to find the cause.  It may be fixed now due to a change I'm
	// also making in UpdateDuckJumpEyeOffset but just in case, this code will sense the 
	// problem and restore the eye to the proper position.  It doesn't smooth the transition,
	// but it is preferable to leaving the player's view too low.
	//
	// If the player is still alive and not an observer, check to make sure that
	// his view height is at the standing height.
	else if ( !IsDead() && !player->IsObserver() && !player->IsInAVehicle() )
	{
		if ( ( player->m_Local.m_nDuckJumpTimeMsecs == 0 ) && ( fabs(player->GetViewOffset().z - GetPlayerViewOffset( false ).z) > 0.1 ) )
		{
			// we should rarely ever get here, so assert so a coder knows when it happens
			AssertMsgOnce( 0, "Restoring player view height\n" );

			// set the eye height to the non-ducked height
			SetDuckedEyeOffset(0.0f);
		}
	}
}


static ConVar sv_optimizedmovement( "sv_optimizedmovement", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalGameMovement::PlayerMove()
{
	VPROF( "CPortalGameMovement::PlayerMove" );

	CheckParameters();

	// clear output applied velocity
	mv->m_outWishVel.Init();
	mv->m_outJumpVel.Init();

	MoveHelper( )->ResetTouchList();                    // Assume we don't touch anything

	ReduceTimers();

	AngleVectors (mv->m_vecViewAngles, &m_vecForward, &m_vecRight, &m_vecUp );  // Determine movement angles

	// Always try and unstick us unless we are using a couple of the movement modes
	MoveType_t moveType = player->GetMoveType();
	if ( moveType != MOVETYPE_NOCLIP && 
		moveType != MOVETYPE_NONE && 		 
		moveType != MOVETYPE_ISOMETRIC && 
		moveType != MOVETYPE_OBSERVER && 
		!player->pl.deadflag )
	{
		if ( CheckInterval( STUCK ) )
		{
			if ( CheckStuck() )
			{
				// Can't move, we're stuck
				return;  
			}
		}
	}

	// Now that we are "unstuck", see where we are (player->GetWaterLevel() and type, player->GetGroundEntity()).
	if ( player->GetMoveType() != MOVETYPE_WALK ||
		mv->m_bGameCodeMovedPlayer || 
		!sv_optimizedmovement.GetBool()  )
	{
		CategorizePosition();
	}
	else
	{
		if ( -DotProduct( mv->m_vecVelocity, m_vGravityDirection ) > 200.0f )
		{
			SetGroundEntity( NULL );
		}
	}

	// Store off the starting water level
	m_nOldWaterLevel = player->GetWaterLevel();

	// If we are not on ground, store off how fast we are moving down
	if ( player->GetGroundEntity() == NULL )
	{
		player->m_Local.m_flFallVelocity = DotProduct( mv->m_vecVelocity, m_vGravityDirection );
	}

	m_nOnLadder = 0;

	player->UpdateStepSound( player->m_pSurfaceData, mv->GetAbsOrigin(), mv->m_vecVelocity );

	UpdateDuckJumpEyeOffset();
	Duck();

	// Don't run ladder code if dead on on a train
	if ( !player->pl.deadflag && !(player->GetFlags() & FL_ONTRAIN) )
	{
		// If was not on a ladder now, but was on one before, 
		//  get off of the ladder

		// TODO: this causes lots of weirdness.
		//bool bCheckLadder = CheckInterval( LADDER );
		//if ( bCheckLadder || player->GetMoveType() == MOVETYPE_LADDER )
		{
			if ( !LadderMove() && 
				( player->GetMoveType() == MOVETYPE_LADDER ) )
			{
				// Clear ladder stuff unless player is dead or riding a train
				// It will be reset immediately again next frame if necessary
				player->SetMoveType( MOVETYPE_WALK );
				player->SetMoveCollide( MOVECOLLIDE_DEFAULT );
			}
		}
	}

	// Handle movement modes.
	switch (player->GetMoveType())
	{
	case MOVETYPE_NONE:
		break;

	case MOVETYPE_NOCLIP:
		FullNoClipMove( sv_noclipspeed.GetFloat(), sv_noclipaccelerate.GetFloat() );
		break;

	case MOVETYPE_FLY:
	case MOVETYPE_FLYGRAVITY:
		FullTossMove();
		break;

	case MOVETYPE_LADDER:
		FullLadderMove();
		break;

	case MOVETYPE_VPHYSICS:
	case MOVETYPE_WALK:
		FullWalkMove();
		break;

	case MOVETYPE_ISOMETRIC:
		//IsometricMove();
		// Could also try:  FullTossMove();
		FullWalkMove();
		break;

	case MOVETYPE_OBSERVER:
		FullObserverMove(); // clips against world&players
		break;

	default:
		DevMsg( 1, "Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", player->GetMoveType(), player->IsServer());
		break;
	}
}

// Expose our interface.
static CPortalGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
