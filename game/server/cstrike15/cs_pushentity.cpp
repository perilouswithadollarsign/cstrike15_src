//========= Copyright Valve Corporation,k All rights reserved. ============//
//
//
// Note: This code integrated then adapted from TF:
//			//ValveGames/staging/src/game/server/tf/tf_pushentity.cpp
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "pushentity.h"
#include "cs_player.h"
#include "collisionutils.h"
#include "cs_gamerules.h"
//#include "mathlib/mathlib.h"

class CCSPhysicsPushEntities : public CPhysicsPushedEntities
{
public:

	DECLARE_CLASS( CCSPhysicsPushEntities, CPhysicsPushedEntities );

	// Constructor/Destructor.
	CCSPhysicsPushEntities();
	~CCSPhysicsPushEntities();

protected:

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot ) OVERRIDE;
	virtual bool SpeculativelyCheckLinearPush( const Vector &vecAbsPush ) OVERRIDE;
	virtual void FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove ) OVERRIDE;
	
private:

	bool RotationPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, const RotatingPushMove_t &rotPushMove, bool bRotationalPush, CBaseEntity *pRoot );
	bool RotationCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates );
	bool LinearPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush );
	bool LinearCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates );
	void EnsureValidPushWhileRiding( CBaseEntity *pBlocker, CBaseEntity *pPusher );

	bool IsPlayerAABBIntersetingPusherOBB( CBaseEntity *pEntity, CBaseEntity *pRootEntity );

	void MovePlayer( CBaseEntity *pBlocker, PhysicsPushedInfo_t &info, float flMoveScale, bool bPusherIsTrain, bool bIgnoreTeammates );
	void FindNewPushDirection( Vector &vecCurrent, Vector &vecNormal, Vector &vecOutput );

	float m_flPushDist;
	Vector	m_vecPushVector;
};

CCSPhysicsPushEntities s_CSPushedEntities;
CPhysicsPushedEntities *g_pPushedEntities = &s_CSPushedEntities;

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CCSPhysicsPushEntities::CCSPhysicsPushEntities()
{

}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CCSPhysicsPushEntities::~CCSPhysicsPushEntities()
{

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	Vector vecAbsPush( 0.0f, 0.0f, 0.0f );
	m_nBlocker = -1;
	int nMovedCount = m_rgMoved.Count();
	for ( int i = ( nMovedCount - 1 ); i >= 0; --i )
	{
		// Is the entity and CS Player?
		CCSPlayer *pCSPlayer = NULL;
		bool bPusherIsTrain = false;
		if ( m_rgMoved[i].m_pEntity && m_rgMoved[i].m_pEntity->IsPlayer() )
		{
			pCSPlayer = ToCSPlayer( m_rgMoved[i].m_pEntity );
			CBaseEntity* pPusher = m_rgPusher[ 0 ].m_pEntity->GetRootMoveParent();
			bPusherIsTrain = pPusher && pPusher->IsBaseTrain();
		}

		// Special code to move the player away from the func_train.
		// Only do this if it's a train pushing a player--otherwise use base class. 
		if ( pCSPlayer && bPusherIsTrain )
		{
			// Rotationally push the player!
			ComputeRotationalPushDirection( m_rgMoved[ i ].m_pEntity, rotPushMove, &vecAbsPush, pRoot );
			RotationPushCSPlayer( m_rgMoved[i], vecAbsPush, rotPushMove, true, pRoot );
		}
		else
		{
			// Keep this in sync with BaseClass::SpeculativelyCheckRotPush
			ComputeRotationalPushDirection( m_rgMoved[i].m_pEntity, rotPushMove, &vecAbsPush, pRoot );
			if ( !SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, true, pRoot ) )
			{
				m_nBlocker = i;
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::SpeculativelyCheckLinearPush( const Vector &vecAbsPush )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	m_nBlocker = -1;
	int nMovedCount = m_rgMoved.Count();
	for ( int i = ( nMovedCount - 1 ); i >= 0; --i )
	{
		// Is the entity and CS Player?
		CCSPlayer *pCSPlayer = NULL;
		bool bPusherIsTrain = false;
		if ( m_rgMoved[i].m_pEntity && m_rgMoved[i].m_pEntity->IsPlayer() )
		{
			pCSPlayer = ToCSPlayer( m_rgMoved[i].m_pEntity );
			CBaseEntity* pPusher = m_rgPusher[0].m_pEntity->GetRootMoveParent();
 			bPusherIsTrain = pPusher && pPusher->IsBaseTrain();
		}

		// Special code to move the player away from the func_train.
		// Only do this if it's a train pushing a player--otherwise use base class. 
		if ( pCSPlayer && bPusherIsTrain )
		{
			// Linearly push the player!
 			LinearPushCSPlayer( m_rgMoved[i], vecAbsPush, false );
		}
		else
		{
			// Keep this in sync with BaseClass::SpeculativelyCheckLinearPush
			if ( !SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, false, NULL ) )
			{
				m_nBlocker = i;
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::RotationPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, const RotatingPushMove_t &rotPushMove, bool bRotationalPush, CBaseEntity* pRoot )
{
	const bool cbIgnoreTeammates = !CSGameRules() || !CSGameRules()->IsTeammateSolid();
	Assert( cbIgnoreTeammates ); // This code doesn't behave if teammates are solid.

	// Clear out the collision entity so that if we early out we don't send bogus collision data to the physics system.
	info.m_Trace.m_pEnt = NULL;

	// Look into doing a full engine->CM_Clear( trace)

	// Get the player.
	CCSPlayer *pPlayer = ToCSPlayer( info.m_pEntity );
	if ( !pPlayer )
		return false;

	info.m_vecStartAbsOrigin = pPlayer->GetAbsOrigin();

	// Get the player collision data.
	CCollisionProperty *pCollisionPlayer = info.m_pEntity->CollisionProp();
	if ( !pCollisionPlayer )
		return false;

	// Find the root object if in hierarchy.
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pRootEntity )
		return false;

	// This code doesn't at all match the code in AvoidPushawayProps, which is a bummer. It'd be
	// great if this just got rolled into that. Unfortunately, doing so would also require 
	// making trains predictive--they are not currently.
	if ( !pPlayer->GetGroundEntity() || pPlayer->GetGroundEntity()->GetRootMoveParent() != pRootEntity )
	{
		Vector vMinPushAway = vecAbsPush;
		m_flPushDist = VectorNormalize( vMinPushAway );
		m_vecPushVector = vMinPushAway;

		Assert( !m_vecPushVector.IsZero() ); // Is our push vector legit?
	}
	else
	{
		SpeculativelyCheckPush( info, vecAbsPush, true, pRoot, cbIgnoreTeammates );
		EnsureValidPushWhileRiding( pPlayer, pRootEntity );
	}

	return RotationCheckPush( info, cbIgnoreTeammates );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::RotationCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates )
{
	// Get the blocking and pushing entities.
	CBaseEntity *pBlocker = info.m_pEntity;
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pBlocker || !pRootEntity )
		return true;

	int *pPusherHandles = ( int* )stackalloc( m_rgPusher.Count() * sizeof( int ) );
	UnlinkPusherList( pPusherHandles );
	for ( int iPushTry = 0; iPushTry < 3; ++iPushTry )
	{
		MovePlayer( pBlocker, info, 0.35f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
		if ( IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
			break;
	}
	RelinkPusherList( pPusherHandles );

	// Is the blocked ground the push entity?
	info.m_bPusherIsGround = false;
	if ( pBlocker->GetGroundEntity() && pBlocker->GetGroundEntity()->GetRootMoveParent() == m_rgPusher[0].m_pEntity )
	{
		info.m_bPusherIsGround = true;
	}

	// Check to see if the player is in a good spot and attempt a move again if not - but only if it isn't being ridden on.
	if ( !IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
	{
		// Try again is the player is still blocked.
		DevMsg( 2, "Pushing rotation hard!\n" );
		UnlinkPusherList( pPusherHandles );
		MovePlayer( pBlocker, info, 1.0f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
		RelinkPusherList( pPusherHandles );
	}

	// The player will never stop a train from moving in CS.
	info.m_bBlocked = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::LinearPushCSPlayer( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush )
{
	const bool cbIgnoreTeammates = !CSGameRules() || !CSGameRules()->IsTeammateSolid();
	Assert( cbIgnoreTeammates ); // This code doesn't behave if teammates are solid.

	// Clear out the collision entity so that if we early out we don't send bogus collision data to the physics system.
	info.m_Trace.m_pEnt = NULL;

	// Get the player.
	CCSPlayer *pPlayer = ToCSPlayer( info.m_pEntity );
	if ( !pPlayer )
		return false;

	info.m_vecStartAbsOrigin = pPlayer->GetAbsOrigin();

	// Get the player collision data.
	CCollisionProperty *pCollisionPlayer = info.m_pEntity->CollisionProp();
	if ( !pCollisionPlayer )
		return false;

	// Find the root object if in hierarchy.
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pRootEntity )
		return false;

	// Get the pusher collision data.
	CCollisionProperty *pCollisionPusher = pRootEntity->CollisionProp();
	if ( !pCollisionPusher )
		return false;

	if ( !pPlayer->GetGroundEntity() || pPlayer->GetGroundEntity()->GetRootMoveParent() != pRootEntity )
	{
		m_vecPushVector = vecAbsPush;
		m_flPushDist = VectorNormalize( m_vecPushVector );
	}
	else
	{
		// Try to get the base class first. 
		SpeculativelyCheckPush( info, vecAbsPush, false, NULL, cbIgnoreTeammates );
 		EnsureValidPushWhileRiding( pPlayer, pRootEntity );
	}

	return LinearCheckPush( info, cbIgnoreTeammates );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::LinearCheckPush( PhysicsPushedInfo_t &info, bool bIgnoreTeammates )
{
	// Get the blocking and pushing entities.
	CBaseEntity *pBlocker = info.m_pEntity;
	CBaseEntity *pRootEntity = m_rgPusher[0].m_pEntity->GetRootMoveParent();
	if ( !pBlocker || !pRootEntity )
		return true;

	// Unlink the pusher from the spatial partition and attempt a player move.
	int *pPusherHandles = ( int* )stackalloc( m_rgPusher.Count() * sizeof( int ) );
	UnlinkPusherList( pPusherHandles );
	MovePlayer( pBlocker, info, 1.0f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
	RelinkPusherList( pPusherHandles );

	// Is the pusher the ground entity the blocker is standing on?
	info.m_bPusherIsGround = false;
	if ( pBlocker->GetGroundEntity() && pBlocker->GetGroundEntity()->GetRootMoveParent() == m_rgPusher[0].m_pEntity )
	{
		info.m_bPusherIsGround = true;
	}

	// Check to see if the player is in a good spot and attempt a move again if not - but only if it isn't being ridden on.
	if ( !info.m_bPusherIsGround && !IsPushedPositionValid( pBlocker, bIgnoreTeammates ) )
	{
		// Try again is the player is still blocked.
		DevMsg( 2, "Pushing linear hard!\n" );
		UnlinkPusherList( pPusherHandles );
		MovePlayer( pBlocker, info, 1.0f, pRootEntity->IsBaseTrain(), bIgnoreTeammates );
		RelinkPusherList( pPusherHandles );
	}

	// The player will never stop a train from moving in CS.
	info.m_bBlocked = false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: When riding atop a vehicle, try to ensure that the final push vector and 
// push distance will land us in a valid location. 
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::EnsureValidPushWhileRiding( CBaseEntity *pBlocker, CBaseEntity *pPusher )
{
	const bool cbIgnoreTeammates = !CSGameRules() || !CSGameRules()->IsTeammateSolid();
	Assert( cbIgnoreTeammates ); // This code doesn't behave if teammates are solid.
	
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL3 );

	m_vecPushVector.Zero();
	m_flPushDist = 0.0f;

	// Do we still have a collision? 
	if ( IsPushedPositionValid( pBlocker, cbIgnoreTeammates ) )
		return;
	
	const float cMaxDistToLookForPlacement = 72;
	// Try nudging them upwards a bit.
	if ( FindValidLocationUpwards( &m_flPushDist, pBlocker, cMaxDistToLookForPlacement, 1.1 ) )
	{
		m_vecPushVector.z = 1.0f;
	}
	else
	{
		// Try to push the player backwards along the direction of travel of the vehicle (and also up a bit)?
		Vector vBackwardsAndUp( 0, 0, 1 );
		Vector vTraceEndpoint = pPusher->GetAbsVelocity();
		vTraceEndpoint.x = -vTraceEndpoint.x;
		vTraceEndpoint.y = -vTraceEndpoint.y;
		vTraceEndpoint.z = sqrt( vTraceEndpoint.x * vTraceEndpoint.x + vTraceEndpoint.y * vTraceEndpoint.y );
		vTraceEndpoint = vTraceEndpoint.Normalized() * cMaxDistToLookForPlacement;
		vTraceEndpoint += pBlocker->GetAbsOrigin();

		Vector vDelta;
		if ( FindValidLocationAlongVector( &vDelta, pBlocker, vTraceEndpoint, 1.1 ) )
		{
			m_vecPushVector = vDelta;
			m_flPushDist = VectorNormalize( m_vecPushVector );
		}
		else
		{
			Assert( !"Failed to find a location upwards or backwards for a blocker, sadness!" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCSPhysicsPushEntities::IsPlayerAABBIntersetingPusherOBB( CBaseEntity *pEntity, CBaseEntity *pRootEntity )
{
	// Get the player.
	CCSPlayer *pPlayer = ToCSPlayer( pEntity );
	if ( !pPlayer )
		return false;

	// Get the player collision data.
	CCollisionProperty *pCollisionPlayer = pEntity->CollisionProp();
	if ( !pCollisionPlayer )
		return false;

	// Get the pusher collision data.
	CCollisionProperty *pCollisionPusher = pRootEntity->CollisionProp();
	if ( !pCollisionPusher )
		return false;

	// Do we have a collision.
	 return IsOBBIntersectingOBB( pCollisionPlayer->GetCollisionOrigin(), pCollisionPlayer->GetCollisionAngles(), pCollisionPlayer->OBBMins(), pCollisionPlayer->OBBMaxs(), 
		pCollisionPusher->GetCollisionOrigin(), pCollisionPusher->GetCollisionAngles(), pCollisionPusher->OBBMins(), pCollisionPusher->OBBMaxs(), 
		0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::FindNewPushDirection( Vector &vecCurrent, Vector &vecNormal, Vector &vecOutput )
{
	// Determine how far along plane to slide based on incoming direction.
	float flBackOff = DotProduct( vecCurrent, vecNormal );

	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		float flDelta = vecNormal[iAxis] * flBackOff;
		vecOutput[iAxis] = vecCurrent[iAxis] - flDelta; 
	}

	// iterate once to make sure we aren't still moving through the plane
	float flAdjust = DotProduct( vecOutput, vecNormal );
	if( flAdjust < 0.0f )
	{
		vecOutput -= ( vecNormal * flAdjust );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::MovePlayer( CBaseEntity *pBlocker, PhysicsPushedInfo_t &info, float flMoveScale, bool bPusherIsTrain, bool bIgnoreTeammates )
{
	// Find out how far we still need to move.
	float flFractionLeft = 1.0f;
	float flNewDist = m_flPushDist *flMoveScale;
	Vector vecPush = m_vecPushVector;

	// Find a new push vector.
	Vector vecStart = pBlocker->GetAbsOrigin();
	Vector logVecStart = vecStart;
	Vector logVecEnd = vecStart;
	int iSteps = 0;

	vecStart.z += 4.0f;
	for ( int iTest = 0; iTest < 4; ++iTest )
	{
		// Clear the trace entity.
		Vector vecEnd = pBlocker->GetAbsOrigin() + ( flNewDist * vecPush );
		TraceBlockerEntity( pBlocker, vecStart, vecEnd, bIgnoreTeammates, &info.m_Trace );

		if ( info.m_Trace.fraction > 0.0f )
		{
			pBlocker->SetAbsOrigin( info.m_Trace.endpos );
			logVecEnd = info.m_Trace.endpos;
			iSteps = iTest + 1;
		}

		if ( info.m_Trace.fraction == 1.0f || !info.m_Trace.m_pEnt )
			break;

		// New test distance and position.
		flFractionLeft = 1.0f - info.m_Trace.fraction;
		flNewDist = flFractionLeft * flNewDist;
		flNewDist = flNewDist * ( 1.0f + ( 1.0f - fabs( info.m_Trace.plane.normal.Dot( vecPush ) ) ) );

		// Find the new push direction.
		Vector vecTmp;
		FindNewPushDirection( vecPush, info.m_Trace.plane.normal, vecTmp );
		VectorCopy( vecTmp, vecPush );
	}

	Vector finalPushVec = logVecEnd - logVecStart;
	DevMsg( 2, "Pushed player by %.2f over %d steps (push vector: %.2f, %.2f, %.2f)\n", finalPushVec.Length(), iSteps, finalPushVec.x, finalPushVec.y, finalPushVec.z );
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CCSPhysicsPushEntities::FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove )
{
	if ( !pPushedEntity->IsPlayer() )
	{
		QAngle angles = pPushedEntity->GetAbsAngles();

		// only rotate YAW with pushing.  Freely rotateable entities should either use VPHYSICS
		// or be set up as children
		angles.y += rotPushMove.amove.y;
		pPushedEntity->SetAbsAngles( angles );
	}
}
