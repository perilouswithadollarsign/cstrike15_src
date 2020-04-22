//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"
#include "trigger_tractorbeam_shared.h"
#include "in_buttons.h"
#include "portal_util_shared.h"

#include <functional>

#ifdef CLIENT_DLL

#include "c_portal_player.h"
#include "c_paintblob.h"
#include "portal_mp_gamerules.h"

#else

#include "portal_player.h"
#include "npc_portal_turret_floor.h"
#include "prop_weightedcube.h"
#include "cpaintblob.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar sv_gravity;

ConVar tbeam_air_ctrl_threshold( "tbeam_air_ctrl_threshold", "20", FCVAR_REPLICATED | FCVAR_CHEAT );

CTractorBeam_Manager g_TractorBeamManager;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTrigger_TractorBeam::EndTouch( CBaseEntity *pOther )
{
#ifdef GAME_DLL
	if ( !PassesTriggerFilters( pOther ) )
		return;
#endif

	BaseClass::EndTouch( pOther );

#ifdef GAME_DLL
	if ( FClassnameIs( pOther, "npc_portal_turret_floor" ) )
	{
		CNPC_Portal_FloorTurret *pTurret = assert_cast< CNPC_Portal_FloorTurret* >( pOther );
		if ( pTurret )
		{
			pTurret->OnExitedTractorBeam();
		}
	}
	else if ( UTIL_IsReflectiveCube( pOther ) )
	{
		CPropWeightedCube *pCube = assert_cast< CPropWeightedCube* >( pOther );
		if ( pCube )
		{
			pCube->OnExitedTractorBeam();
		}
	}
#endif

	bool bTravelingToLinkedTBeam = false;

	if ( pOther->IsPlayer() )
	{
		CPortal_Player *pPlayer = static_cast< CPortal_Player* >( pOther );
		if ( pPlayer )
		{
			if ( m_hProxyEntity.Get() )
			{
				// Lets see if the player is going through a portal
				Vector vExtents = pPlayer->GetPlayerMaxs() - pPlayer->GetPlayerMins();
				for ( int i=0; i<CPortal_Base2D_Shared::AllPortals.Count(); ++i )
				{
					CPortal_Base2D *pPortal = CPortal_Base2D_Shared::AllPortals[i];
					if ( pPortal && UTIL_IsBoxIntersectingPortal( pPlayer->WorldSpaceCenter(), vExtents, pPortal ) )
					{
						// Is the tbeam is pushing us into the portal that we're traveling through?
						CPortal_Base2D *pPortal2 = IsReversed() ? m_hProxyEntity->GetSourcePortal() : m_hProxyEntity->GetHitPortal();
						if ( pPortal == pPortal2 || pPortal->GetLinkedPortal() == pPortal2 )
						{
							bTravelingToLinkedTBeam = true;
							break;
						}
					}
				}
			}

			pPlayer->SetLeaveTractorBeam( this, bTravelingToLinkedTBeam );
		}

#ifdef GAME_DLL
		if ( m_sndPlayerInBeam )
		{
			CSoundEnvelopeController::GetController().SoundFadeOut( m_sndPlayerInBeam, 0.5f );
		}
#endif
	}
	else
	{
#ifdef GAME_DLL
		triggerevent_t event;
		if ( PhysGetTriggerEvent( &event, this ) && event.pObject && m_pController )
		{
			event.pObject->Wake();
			m_pController->DetachObject( event.pObject );
		}
#else
		//Warning( "client EndTouch %f %s %s\n", gpGlobals->curtime, prediction->InPrediction() ? "true" : "false", prediction->IsFirstTimePredicted() ? "First" : "Repredict" );

		if( m_pController )
		{
			IPhysicsObject *pPhysObject = pOther->VPhysicsGetObject();
			if( pPhysObject )
			{
				m_pController->DetachObject( pPhysObject );
			}
		}
#endif

		EntityBeamHistory_t& beamHistory = g_TractorBeamManager.GetHistoryFromEnt( pOther );
		beamHistory.LeaveBeam( this );
#ifdef GAME_DLL
		if ( beamHistory.m_beams.Count() && beamHistory.m_beams.Head().m_hBeamHandle != NULL )
		{
			static_cast< CTrigger_TractorBeam* >( beamHistory.m_beams.Head().m_hBeamHandle.Get() )->ForceAttachEntity( pOther );
		}
#endif
	}
}


void CTrigger_TractorBeam::UpdateBeam( const Vector& vStartPoint, const Vector& vEndPoint, float flLinearForce )
{
	CBaseEntity *pOwner = GetOwnerEntity();
	if ( !pOwner )
		return;

#if defined( GAME_DLL )
	// We want to touch everything...
	AddSpawnFlags( SF_TRIGGER_ALLOW_ALL );
#endif

	if ( flLinearForce < 0 )
	{
		flLinearForce = fabs(flLinearForce);
#if defined( GAME_DLL )
		SetAsReversed( true );
#endif
		SetDirection( vEndPoint, vStartPoint );
	}
	else
	{
#if defined( GAME_DLL )
		SetAsReversed( false );
#endif
		SetDirection( vStartPoint, vEndPoint );
	}

	Vector vStart = GetStartPoint();
	Vector vEnd = GetEndPoint();

	// Get our local vectors
	Vector vDir = ( vEnd - vStart );
	float flLength = vDir.NormalizeInPlace();

	QAngle qBeamAngles;
	VectorAngles( vDir, qBeamAngles );

	// Setup our base (unrotated) bounding box
	const float flHalfWidth = GetBeamRadius();
	Vector vMins( 0,		-flHalfWidth, -flHalfWidth );
	Vector vMaxs( flLength,  flHalfWidth,  flHalfWidth );

	// Clear any current physobject we have assigned to us
	IPhysicsObject *pObject = VPhysicsGetObject();
	if ( pObject )
	{
		// Untouch everything
		pObject->RemoveTrigger();

		// Recreate static physics box
		VPhysicsDestroyObject();
	}

	// Create our object
	if ( physenv )
	{
		IPhysicsObject *pPhysicsObject = PhysModelCreateOBB( this, vMins, vMaxs, m_vStart, qBeamAngles, true );
		VPhysicsSetObject( pPhysicsObject );
		pPhysicsObject->BecomeTrigger();

		// make sure origin and angle are todate to compute world surrounding bounds
		SetAbsOrigin( m_vStart );
		SetAbsAngles( qBeamAngles );

		CollisionProp()->SetCollisionBounds( vMins, vMaxs );
		CollisionProp()->SetSurroundingBoundsType( USE_OBB_COLLISION_BOUNDS );

#ifdef CLIENT_DLL
		// need to add the entity to partition tree so we can trace against this on the client
		Vector vWorldMins, vWorldMaxs;
		CollisionProp()->WorldSpaceSurroundingBounds( &vWorldMins, &vWorldMaxs );
		::partition->ElementMoved( CollisionProp()->GetPartitionHandle(), vWorldMins, vWorldMaxs );
#endif
	}

#if defined( GAME_DLL )
	// Setup our direction and force data
	SetLinearForce( vDir, flLinearForce );
	SetLinearLimit( flLinearForce * 0.5f );
#endif

#if defined( GAME_DLL )
	// Now wake everything up
	WakeTouchingObjects();
#endif

#if defined( CLIENT_DLL )
	// Deal with particle changes
	CreateParticles();
#endif // CLIENT_DLL

	RemoveAllBlobsFromBeam();

	m_nLastUpdateFrame = gpGlobals->framecount;
}


int CTrigger_TractorBeam::GetLastUpdateFrame() const
{
	return m_nLastUpdateFrame;
}


//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CTrigger_TractorBeam::SetDirection( const Vector &vStart, const Vector &vEnd )
{
	// Cache these points for the server / client
	m_vStart = vStart;
	m_vEnd = vEnd;
}

Vector CTrigger_TractorBeam::GetForceDirection() const
{
	Vector vecForceDir = GetEndPoint() - GetStartPoint();
	return vecForceDir.Normalized();
}

ConVar tbeam_allow_player_struggle( "tbeam_allow_player_struggle", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar tbeam_prevent_players_from_colliding( "tbeam_prevent_players_from_colliding", "1", FCVAR_REPLICATED | FCVAR_CHEAT );

//------------------------------------------------------------------------------
// Apply the forces to the entity
//------------------------------------------------------------------------------
IMotionEvent::simresult_e CTrigger_TractorBeam::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	if ( m_bDisabled )
		return SIM_NOTHING;

	linear.Init();
	angular.Init();

	// Don't affect things held by the player
	if ( !pObject || (pObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD) )
		return SIM_NOTHING;

	// Get our actual game entity
	CBaseEntity *pEntity = static_cast<CBaseEntity *>(pObject->GetGameData());

	CalculateFrameMovement( pObject, pEntity, deltaTime, linear, angular );

	return SIM_GLOBAL_ACCELERATION;
}

void CTrigger_TractorBeam::CalculateFrameMovement( IPhysicsObject *pObject, CBaseEntity *pEntity, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	// We want to influence entities to rest at our "middles" so that they pass through portals and are predictable.  
	// Here we perturb the position of the entity in question to move it towards the "middle line" of our beam.
	Vector vWorldPos;
	Vector vel;
	AngularImpulse angVel;
	if ( pObject )
	{
		pObject->GetPosition( &vWorldPos, NULL );
		pObject->GetVelocity( &vel, &angVel );
	}
	else
	{
		vWorldPos = pEntity->WorldSpaceCenter();
		vel = pEntity->GetAbsVelocity();
		angVel.Init();
	}

	const bool bIsPlayer = pEntity->IsPlayer();

	// Find the direction of travel
	Vector vecForceDir = GetForceDirection();

	float flLinearScale = 1.0f;
	Vector vCenteringPos = vWorldPos + vel * deltaTime * 5.0f; // Use the position that we're heading toward to dampen oscelation as we approach the center
	Vector vMidPoint;
	float flPathPerc; // How far along the beam we are
	CalcClosestPointOnLineSegment( vCenteringPos, m_vStart, m_vEnd, vMidPoint, &flPathPerc );

	CBasePlayer *pPlayer = NULL;
	if ( bIsPlayer )
	{
		pPlayer = ToBasePlayer( pEntity );

		// Players need to decelerate when they reach the end of the trigger (unless they're going to go through a portal!)
		bool bSourcePortal = m_hProxyEntity.Get() ? m_hProxyEntity->GetSourcePortal() != NULL : false;
		bool bHitPortal = m_hProxyEntity.Get() ? m_hProxyEntity->GetHitPortal() != NULL : false;
		bool bHeadingTowardsPortal = ( m_bReversed ) ? (bool) bSourcePortal : (bool) bHitPortal;

		// Players also need to decelerate if they'll bump into each other
		bool bMayBumpIntoOtherPlayerInTbeam = false;
		float flPlayerBlockingPathPerc = 0.0f;
		float flBlockedBeamLength = 0.0f;
		if( tbeam_prevent_players_from_colliding.GetBool() && GameRules()->IsMultiplayer() )
		{
			CPortal_Player const* pOtherPlayer = assert_cast<CPortal_Player*>( UTIL_OtherPlayer( pPlayer ) );

			if( pOtherPlayer && pOtherPlayer->GetTractorBeam() == this )
			{
				// Compute the percentage along the beam of the other player
				Vector const vOtherCenteringPos = pOtherPlayer->WorldSpaceCenter() + pOtherPlayer->GetAbsVelocity() * deltaTime * 5.0f;
				Vector vOtherClosest;
				float flOtherPathPerc;
				CalcClosestPointOnLineSegment( vOtherCenteringPos, m_vStart, m_vEnd, vOtherClosest, &flOtherPathPerc );

				// This player might bump into the other if she is behind the other player.
				// If they're both at the same point in the beam, slow one down temporarily.
				bMayBumpIntoOtherPlayerInTbeam = flPathPerc == flOtherPathPerc ? pPlayer->entindex() > pOtherPlayer->entindex() : flPathPerc < flOtherPathPerc;

				// Reuse the code below to slow down the player. Essentially, if both players are in
				// the same beam, we're treating the player that's in front as the end point of the
				// beam for the player that's behind.
				if( bMayBumpIntoOtherPlayerInTbeam )
				{
					float const PLAYER_DISTANCE_BUFFER = 18.0f;	// Approximately how far the gun can extend outside the AABB
					Vector temp;
					Vector const end = vOtherClosest - PLAYER_DISTANCE_BUFFER * vecForceDir;
					CalcClosestPointOnLineSegment( vCenteringPos, m_vStart, end, temp, &flPlayerBlockingPathPerc );
					flBlockedBeamLength = (end - m_vStart).Length();
				}
			}
		}

		if ( (bHeadingTowardsPortal == false && !m_bDisablePlayerMove) || bMayBumpIntoOtherPlayerInTbeam )
		{
			// Find the player's "width" in the direction travel (different when moving up/down to left/right)
			Vector mins, maxs;
			pEntity->CollisionProp()->NormalizedToCollisionSpace( vec3_origin, &mins );
			pEntity->CollisionProp()->NormalizedToCollisionSpace( vecForceDir, &maxs );

			float flRadius = ( maxs - mins ).Length();

			// Find where we are along the length of the beam
			float flBeamLength = bMayBumpIntoOtherPlayerInTbeam ? flBlockedBeamLength : VectorLength( (Vector)m_vEnd - (Vector)m_vStart );
			float flEndPerc = ( flRadius / flBeamLength );

			float flRemapPathPerc = bMayBumpIntoOtherPlayerInTbeam ? flPlayerBlockingPathPerc : flPathPerc;
			
			// If we're within that last period, we need to scale down the linear force
			if ( flRemapPathPerc >= flEndPerc )
			{
				flLinearScale = RemapValClamped( flRemapPathPerc, (1.0f-(flEndPerc*2.0f)), (1.0f-flEndPerc), 1.0f, 0.0f );
			}
		}
	}

	// Apply our constant directional force
	VectorMA( linear, ((m_linearForce*2.0f) * flLinearScale), vecForceDir, linear );

	// Deal with limiting velocity
	if ( HasAirDensity() || HasLinearLimit() || HasLinearScale() || HasAngularLimit() || HasAngularScale() )
	{
		vel += linear * deltaTime; // account for gravity scale

		Vector unitVel = vel;
		Vector unitAngVel = angVel;

		float speed = VectorNormalize( unitVel );
		float angSpeed = VectorNormalize( unitAngVel );

		float speedScale = 0.0;
		float angSpeedScale = 0.0;

		if ( HasAirDensity() && pObject )
		{
			float linearDrag = -0.5 * m_addAirDensity * pObject->CalculateLinearDrag( unitVel ) * deltaTime;
			if ( linearDrag < -1 )
			{
				linearDrag = -1;
			}
			speedScale += linearDrag / deltaTime;

			float angDrag = -0.5 * m_addAirDensity * pObject->CalculateAngularDrag( unitAngVel ) * deltaTime;
			if ( angDrag < -1 )
			{
				angDrag = -1;
			}
			angSpeedScale += angDrag / deltaTime;
		}
		if ( HasLinearLimit() && speed > (m_linearLimit*flLinearScale) )
		{
			float flDeltaVel = ((GetLinearLimit()*flLinearScale)- speed) / deltaTime;
			VectorMA( linear, flDeltaVel, unitVel, linear );
		}
		if ( HasAngularLimit() && angSpeed > m_angularLimit )
		{
			angular += ((m_angularLimit - angSpeed)/deltaTime) * unitAngVel;
		}
		if ( HasLinearScale() )
		{
			speedScale = ( (speedScale+1) * m_linearScale ) - 1;
		}
		if ( HasAngularScale() )
		{
			angSpeedScale = ( (angSpeedScale+1) * m_angularScale ) - 1;
		}
		linear += vel * speedScale;
		angular += angVel * angSpeedScale;
	}

	// Push us slowly towards the center and keep us there
	Vector vShoveDir = ( vMidPoint - vCenteringPos );
	float flDistFromCenter = VectorNormalize( vShoveDir );
	float flPerc = RemapValClamped( flDistFromCenter, 0, 32.0f, 0.0f, 1.0f );

	if ( pEntity && bIsPlayer )
	{
		bool bPlayerAirControlling = false;
		Vector vForward, vRight, vUp;
		AngleVectors( pEntity->EyeAngles(), &vForward, &vRight, &vUp );

		Vector vSubDir = vec3_origin;

		float flAirControlMod = 1.0f;

		const CUserCmd *ucmd = pPlayer->GetLastUserCommand();
		if ( ucmd && !m_bDisablePlayerMove )
		{
			vSubDir = ( vForward * ucmd->forwardmove ) + ( vRight * ucmd->sidemove ) + ( vUp * ucmd->upmove );
			bPlayerAirControlling = ( VectorNormalize( vSubDir ) > tbeam_air_ctrl_threshold.GetFloat() );

			// Don't let them swim upstream
			float flTravelDot = DotProduct( vSubDir, vecForceDir );

			// This is an old deprecated way of letting the player move through tractor beams
			if ( tbeam_allow_player_struggle.GetBool() )
			{
				if ( flTravelDot < 0.0f )
				{
					vSubDir -= flTravelDot * vecForceDir;				
				}

				// If the beam is vertical, don't let players' velocities get limited by looking down
				if ( fabs( vecForceDir.z ) > DOT_45DEGREE )
				{
					VectorNormalize( vSubDir );
				}
			}
			else
			{
				// Clip the movement so they can't swim up or down the stream
				vSubDir -= flTravelDot * vecForceDir;

				if ( vSubDir.z > 0.0f && vCenteringPos.z - vMidPoint.z > 30.0f )
				{
					// Don't let them climb up above the tbeam
					vSubDir.z = 0.0f;
				}

				// Find out how much we're going to throttle their movement speed based on their view direction
				flAirControlMod = RemapValClamped( fabs(flTravelDot), 1.0f, DOT_30DEGREE, 0.0f, 1.0f );
			}
		}

		if ( m_bDisablePlayerMove )
		{
			pPlayer->ForceButtons( IN_DUCK );
			pPlayer->SetGroundEntity( NULL );
		}

		if ( bPlayerAirControlling )
		{
			if ( tbeam_allow_player_struggle.GetBool() )
			{
				linear += vSubDir * 64.0f / deltaTime;			
			}
			else
			{
				Vector vShoveAdd = vShoveDir * ( ( 24.0f * flPerc ) / deltaTime ) * flLinearScale;

				// If we don't allow them to struggle against the beam, then limit them
				if ( tbeam_allow_player_struggle.GetBool() == false )
				{
					vShoveAdd += vSubDir * ( ( 84.0f / deltaTime ) * flAirControlMod );
				}

				linear += vShoveAdd;
			}
		}
		else
		{
			linear += vShoveDir * ( ( 24.0f * flPerc ) / deltaTime ) * flLinearScale;
		}

		// Set the player up to move this way
		if ( pEntity->GetFlags() & FL_ONGROUND )
		{
			pEntity->SetGroundEntity( NULL );
			Vector origin = pEntity->GetAbsOrigin();
			origin.z += 1.0f;
			pEntity->SetAbsOrigin( origin );
			pEntity->AddFlag( FL_BASEVELOCITY );
			pEntity->SetBaseVelocity( Vector( 0, 0, 1000 ) );
			linear.z += 5000.0f;
		}

#if defined( DEBUG_MOTION_CONTROLLERS )
		((CBasePlayer *)pEntity)->m_Debug_LinearAccel = linear;
#endif
	}
	else
	{
		linear += vShoveDir * ( ( 16.0f * flPerc ) / deltaTime );
		linear.z -= (m_gravityScale-1) * sv_gravity.GetFloat();
	}
}


float CTrigger_TractorBeam::GetLinearLimit()
{
	if ( m_linearLimitTime == 0.0f )
		return m_linearLimit;

	float dt = gpGlobals->curtime - m_linearLimitStartTime;
	if ( dt >= m_linearLimitTime )
	{
		m_linearLimitTime = 0.0;
		return m_linearLimit;
	}

	dt /= m_linearLimitTime;
	float flLimit = RemapVal( dt, 0.0f, 1.0f, m_linearLimitStart, m_linearLimit );
	return flLimit;
}


struct ShouldDeleteBlob_t : std::unary_function< CPaintBlob*, bool >
{
	inline bool operator()( const CPaintBlob* pBlob ) const
	{
		return pBlob->ShouldDeleteThis();
	}
};

// Remove dead blobs from the list
void CTrigger_TractorBeam::RemoveDeadBlobs()
{
	if ( m_blobs.Count() == 0 )
		return;

	CPaintBlob** begin = GetBegin( m_blobs );
	CPaintBlob** end = GetEnd( m_blobs );
	CPaintBlob** middle = std::partition( begin, end, ShouldDeleteBlob_t() );

	int numRemoved = middle - begin;
	m_blobs.RemoveMultipleFromHead( numRemoved );
}


struct IsBlobInSameBeam_t : std::unary_function< CPaintBlob*, bool >
{
	IsBlobInSameBeam_t( CTrigger_TractorBeam* pBeam ) : m_pBeam( pBeam )
	{
	}

	inline bool operator()( const CPaintBlob* pBlob ) const
	{
		return ( m_pBeam == pBlob->GetCurrentBeam() );
	}

	CTrigger_TractorBeam *m_pBeam;
};


// remove blobs that change to different beams
void CTrigger_TractorBeam::RemoveChangedBeamBlobs()
{
	if ( m_blobs.Count() == 0 )
		return;

	CPaintBlob** begin = GetBegin( m_blobs );
	CPaintBlob** end = GetEnd( m_blobs );
	CPaintBlob** middle = std::partition( begin, end, IsBlobInSameBeam_t( this ) );

	int numRemoved = end - middle;
	m_blobs.RemoveMultipleFromTail( numRemoved );
}


void CTrigger_TractorBeam::RemoveAllBlobsFromBeam()
{
	for ( int i=0; i<m_blobs.Count(); ++i )
	{
		m_blobs[i]->SetTractorBeam( NULL );
	}

	m_blobs.Purge();
}


void CTrigger_TractorBeam_Shared::RemoveDeadBlobsFromBeams()
{
	for ( int i = 0; i < ITriggerTractorBeamAutoList::AutoList().Count(); ++i )
	{
		static_cast< CTrigger_TractorBeam* >( ITriggerTractorBeamAutoList::AutoList()[i] )->RemoveDeadBlobs();
	}
}


void CTrigger_TractorBeam_Shared::RemoveBlobsFromPreviousBeams()
{
	for ( int i = 0; i < ITriggerTractorBeamAutoList::AutoList().Count(); ++i )
	{
		static_cast< CTrigger_TractorBeam* >( ITriggerTractorBeamAutoList::AutoList()[i] )->RemoveChangedBeamBlobs();
	}
}


bool EntityBeamHistory_t::IsDifferentBeam( CTrigger_TractorBeam* pNewBeam )
{
#ifdef CLIENT_DLL
	// run the old behavior if PC client is playing against PS3 server
	if ( PortalMPGameRules() && !PortalMPGameRules()->IsClientCrossplayingPCvsPC() )
	{
		if ( m_beams.Count() == 0 )
		{
			return true;
		}
		return m_beams.Head().m_hBeamHandle != pNewBeam;
	}
#endif // CLIENT_DLL

	int nLastUpdateFrame = pNewBeam->GetLastUpdateFrame();
	for ( int i=0; i<m_beams.Count(); ++i )
	{
		if ( m_beams[i].m_hBeamHandle == pNewBeam )
		{
			return m_beams[i].m_nLastFrameUpdate != nLastUpdateFrame;
		}
	}

	return true;
}

