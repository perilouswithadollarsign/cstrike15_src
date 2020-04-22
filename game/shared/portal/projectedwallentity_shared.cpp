//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
//===========================================================================//
#include "cbase.h"

#include "paint_power_user.h"

#ifdef CLIENT_DLL
#include "c_projectedwallentity.h"
#include "c_portal_player.h"
#include "c_physicsprop.h"
#define CPhysicsProp C_PhysicsProp
#else
#include "projectedwallentity.h"
#include "portal_player.h"
#include "props.h"					// for CPhysicsProp def

extern void WallPainted( int colorIndex, int nSegment, CBaseEntity *pWall );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#if defined( GAME_DLL )
ConVar wall_debug_time("wall_debug_time", "5.f");
ConVar wall_debug("wall_debug", "0");
#endif

ConVar debug_paintable_projected_wall("debug_paintable_projected_wall", "0", FCVAR_REPLICATED);
ConVar sv_thinnerprojectedwalls( "sv_thinnerprojectedwalls", "0", FCVAR_CHEAT | FCVAR_REPLICATED );

void CProjectedWallEntity::Touch( CBaseEntity* pOther )
{
	//Check if the touched entity is a paint power user
	IPaintPowerUser* pPowerUser = dynamic_cast< IPaintPowerUser* >( pOther );
	if( engine->HasPaintmap() && pPowerUser )
	{
		//Get the up vector of the wall
		Vector vecWallUp;
#if defined( GAME_DLL )
		AngleVectors( GetLocalAngles(), NULL, NULL, &vecWallUp );
#else
		AngleVectors( GetNetworkAngles(), NULL, NULL, &vecWallUp );
#endif

		const trace_t& trace = BaseClass::GetTouchTrace();
		float flDot = DotProduct( vecWallUp, trace.plane.normal );

		//Get the segment of the wall that the power user touched
		Vector vecWorldSpaceCenter = pOther->WorldSpaceCenter();

		Vector vecTouchPoint = UTIL_ProjectPointOntoPlane( vecWorldSpaceCenter, trace.plane );

		const int nSegment = ComputeSegmentIndex( vecTouchPoint );

		if( nSegment >= m_nNumSegments )
		{
			return;
		}

		//Get the paint power at the current segment
		PaintPowerType power = m_PaintPowers[nSegment];

		if( debug_paintable_projected_wall.GetBool() )
		{
			DevMsg( "Segment: %d, Power: %d\n", nSegment, power );
		}

		// We dont want to give power to the user if they're touching the side of a projected wall
		if( !CloseEnough( flDot, 0.0f ) )
		{
			pPowerUser->AddSurfacePaintPowerInfo( PaintPowerInfo_t( trace.plane.normal,
																	trace.endpos,
																	this,
																	power ) );
		}
	}
}


int CProjectedWallEntity::ComputeSegmentIndex( const Vector& vWorldPositionOnWall ) const
{
	const Vector& startPoint = m_vecStartPoint;
	const Vector& endPoint = m_vecEndPoint;

	const Vector wallVector = endPoint - startPoint;
	const Vector contactOffset = vWorldPositionOnWall - startPoint;

	const float distance = DotProduct( wallVector, contactOffset ) / m_flLength;
	Assert( distance + 0.5f > 0.0f && distance < m_flLength + 0.5f );
	return clamp( distance / m_flSegmentLength, 0, m_nNumSegments - 1 );
}


PaintPowerType CProjectedWallEntity::GetPaintPowerAtPoint( const Vector& worldContactPt ) const
{
	return m_PaintPowers[ComputeSegmentIndex(worldContactPt)];
}


void CProjectedWallEntity::Paint( PaintPowerType type, const Vector& worldContactPt )
{	
	const int nSegment = ComputeSegmentIndex( worldContactPt );
	if( nSegment < m_PaintPowers.Count() )
	{
		m_PaintPowers[nSegment] = type;

		#ifndef CLIENT_DLL
		//Send the event to the client
		WallPainted( type, nSegment, this );
		#endif
	}
}


void CProjectedWallEntity::CleansePaint()
{
	for( int i = 0; i < m_nNumSegments; ++i )
	{
// come back to this - MTW
		/*
#if defined( CLIENT_DLL )
		if ( m_PaintPowers[i] != NO_POWER && m_PaintParticles[i] && m_PaintParticles[i]->IsValid() )
		{
			ParticleProp()->StopEmissionAndDestroyImmediately( m_PaintParticles[i] );
		}
		m_PaintParticles[i] = NULL;
#endif
		*/

		m_PaintPowers[i] = NO_POWER;
	}
}


class CProjectorCollideList : public IEntityEnumerator
{
public:
	CProjectorCollideList( Ray_t *pRay, CProjectedWallEntity* pIgnoreEntity, int nContentsMask ) : 
		m_Entities( 0, 32 ), m_pIgnoreEntity( pIgnoreEntity ),
		m_nContentsMask( nContentsMask ), m_pRay(pRay) {}

		virtual bool EnumEntity( IHandleEntity *pHandleEntity )
		{
			// Don't bother with the ignore entity.
			if ( pHandleEntity == m_pIgnoreEntity )
				return true;

			Assert( pHandleEntity );
			if ( !pHandleEntity )
				return true;

#if defined( GAME_DLL )
			CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#else
			CBaseEntity *pEntity = C_BaseEntity::Instance( pHandleEntity->GetRefEHandle() );
#endif

			Assert( pEntity );
			if ( !pEntity )
				return true;

			// Only interested in physics objects, the player and turret npcs
			if ( pEntity->IsPlayer() ||
				dynamic_cast<CPhysicsProp*>( pEntity ) ) 
			{
				Ray_t ray;
				ray.Init( pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs() ) ;
				trace_t tr;
				m_pIgnoreEntity->TestCollision( ray, MASK_SHOT, tr );

				// add if their AABB is overlapping the collideable
				if ( tr.DidHit() )
				{
					m_Entities.AddToTail( pEntity );
				}
			}

			return true;
		}

		CUtlVector<CBaseEntity*>	m_Entities;

private:
	CProjectedWallEntity	*m_pIgnoreEntity;
	int						m_nContentsMask;
	Ray_t					*m_pRay;
};

void CProjectedWallEntity::DisplaceObstructingEntity( CBaseEntity *pEntity, bool bIgnoreStuck )
{
#if defined( GAME_DLL )
	Vector vOrigin = GetLocalOrigin();
#else
	Vector vOrigin = GetNetworkOrigin();
#endif

	Vector vWallForward, vWallRight, vWallUp;
	GetVectors( &vWallForward, &vWallRight, &vWallUp );

	Ray_t ray;
	Vector vLength = GetLengthVector();
	Vector vWallSweptBoxMins, vWallSweptBoxMaxs;
	GetExtents( vWallSweptBoxMins, vWallSweptBoxMaxs );
	ray.Init( vOrigin, vOrigin + vWallForward*vLength.Length(), vWallSweptBoxMins, vWallSweptBoxMaxs );

	CTraceFilterOnlyHitThis filter( pEntity );
	trace_t tr;
	UTIL_TraceRay( ray, MASK_ALL, &filter, &tr );

	if ( tr.DidHit() )
	{
		DisplaceObstructingEntity( pEntity, vOrigin, vWallUp, vWallRight, bIgnoreStuck );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attempts to smooth over the frequent physics-stuck properties of the wall.
//-----------------------------------------------------------------------------
void CProjectedWallEntity::DisplaceObstructingEntities( void )
{
	// Walls size needs to be set up before we test for obstructing objects
	Assert ( !VectorsAreEqual( m_vWorldSpace_WallMins, m_vWorldSpace_WallMaxs ) );

#if defined( GAME_DLL )
	Vector vOrigin = GetLocalOrigin();
#else
	Vector vOrigin = GetNetworkOrigin();
#endif

	Vector vWallForward, vWallRight, vWallUp;
	GetVectors( &vWallForward, &vWallRight, &vWallUp );

	Ray_t ray;
	Vector vLength = GetLengthVector();
	Vector vWallSweptBoxMins, vWallSweptBoxMaxs;
	GetExtents( vWallSweptBoxMins, vWallSweptBoxMaxs );
	ray.Init( vOrigin, vOrigin + vWallForward*vLength.Length(), vWallSweptBoxMins, vWallSweptBoxMaxs );

	CProjectorCollideList enumerator( &ray, this, MASK_SHOT );
	enginetrace->EnumerateEntities( ray, false, &enumerator );

	for( int iEntity = enumerator.m_Entities.Count(); --iEntity >= 0; )
	{
		CBaseEntity *pEntity = enumerator.m_Entities[iEntity];

		DisplaceObstructingEntity( pEntity, vOrigin, vWallUp, vWallRight, false );
	}
}

CEG_NOINLINE void CProjectedWallEntity::DisplaceObstructingEntity( CBaseEntity *pEntity, const Vector &vOrigin, const Vector &vWallUp, const Vector &vWallRight, bool bIgnoreStuck )
{
#ifdef CLIENT_DLL
	if ( !pEntity->GetPredictable() )
		return;
#endif

	Vector vObstructionMaxs	= pEntity->CollisionProp()->OBBMins();
	Vector vObstructionMins	= pEntity->CollisionProp()->OBBMaxs();

	Vector vNewPos = pEntity->GetAbsOrigin();
	QAngle vNewAngles = pEntity->GetAbsAngles();
	Vector vNewVel = pEntity->GetAbsVelocity();

	// TODO:
	// - get 8 corner points out of the OBB
	// - find max distances PointVSPlane for both sides of the plane
	// - use the least distance of the two maxs as distance to push off the entity in the direction of normal of the greater max
	Vector vEntForward, vEntRight, vEntUp;
	AngleVectors( pEntity->CollisionProp()->GetCollisionAngles(), &vEntForward, &vEntRight, &vEntUp );

	Vector ptOBBCenter = pEntity->CollisionProp()->GetCollisionOrigin() + pEntity->CollisionProp()->OBBCenter();
	Vector vExtents = ( vObstructionMaxs - vObstructionMins ) * 0.5f;
	vEntForward *= vExtents.x;
	vEntRight *= vExtents.y;
	vEntUp *= vExtents.z;

	Vector ptOBB[8];
	ptOBB[0] = ptOBBCenter - vEntForward - vEntRight - vEntUp;
	ptOBB[1] = ptOBBCenter - vEntForward - vEntRight + vEntUp;
	ptOBB[2] = ptOBBCenter - vEntForward + vEntRight + vEntUp;
	ptOBB[3] = ptOBBCenter - vEntForward + vEntRight - vEntUp;
	ptOBB[4] = ptOBBCenter + vEntForward - vEntRight - vEntUp;
	ptOBB[5] = ptOBBCenter + vEntForward - vEntRight + vEntUp;
	ptOBB[6] = ptOBBCenter + vEntForward + vEntRight + vEntUp;
	ptOBB[7] = ptOBBCenter + vEntForward + vEntRight - vEntUp;

#if defined( GAME_DLL )
	if ( wall_debug.GetBool() )
	{
		NDebugOverlay::Sphere( ptOBBCenter, 5, 255, 0, 0, true, wall_debug_time.GetFloat() );
		for ( int i=0; i<8; ++i )
		{
			NDebugOverlay::Sphere( ptOBB[i], 2, 255, 0, 0, true, wall_debug_time.GetFloat() );
		}

		NDebugOverlay::VertArrow( GetAbsOrigin(), GetAbsOrigin() + 50.f*vWallUp, 2, 255, 0, 0, 128, true, wall_debug_time.GetFloat() );
	}
	CEG_PROTECT_MEMBER_FUNCTION( CProjectedWallEntity_DisplaceObstructingEntity );
#endif

	VPlane plWallPlane( vWallUp, DotProduct( vWallUp, vOrigin ) );
	float flFrontMax = 0.f;
	float flBackMax = 0.f;
	Vector vFrontMaxPos, vBackMaxPos;
	for ( int i=0; i<8; ++i )
	{
		float flDistToPlane = fabsf( plWallPlane.DistTo( ptOBB[i] ) );
		if ( plWallPlane.GetPointSide( ptOBB[i] ) == SIDE_FRONT )
		{
			if ( flDistToPlane > flFrontMax )
			{
				flFrontMax = flDistToPlane;
				vFrontMaxPos = ptOBB[i];
			}
		}
		else
		{
			if ( flDistToPlane > flBackMax )
			{
				flBackMax = flDistToPlane;
				vBackMaxPos = ptOBB[i];
			}
		}
	}

	// always try to push the entity up or down along Z-axis if the wall is horizontal (walkable)
	// else push the entity to the side of the bridge in the direction of bridge UP vector projected onto XY-plane
	float flHalfWallWidth = m_flWidth / 2.f;
	Vector side1 = vOrigin + flHalfWallWidth * vWallRight;
	Vector side2 = vOrigin - flHalfWallWidth * vWallRight;

	Vector vBumpAxis;
	float flBumpAmount;
	float flInvBumpAmount;
	if ( m_bIsHorizontal )
	{
		vBumpAxis = Vector( 0, 0, 1 );

		// compute the bump amount
		float flDot = fabs( clamp( DotProduct( vWallUp, vBumpAxis ), -1.f, 1.f ) );
		Assert( flDot != 0.0f );
		flBumpAmount = MIN( flBackMax / flDot, MAX( fabs( DotProduct( side1 - vBackMaxPos, vBumpAxis ) ), fabs( DotProduct( side2 - vBackMaxPos, vBumpAxis ) ) ) );
		flInvBumpAmount = MIN( flFrontMax / flDot, MAX( fabs( DotProduct( side1 - vFrontMaxPos, vBumpAxis ) ), fabs( DotProduct( side2 - vFrontMaxPos, vBumpAxis ) ) ) );

		if ( vWallUp.z < 0.f )
		{
			V_swap( flBumpAmount, flInvBumpAmount );
		}
	}
	else
	{
		vBumpAxis = Vector( vWallUp.x, vWallUp.y, 0.f );
		VectorNormalize( vBumpAxis );

		// compute the bump amount
		float flDot = fabs( clamp( DotProduct( vWallUp, vBumpAxis ), -1.f, 1.f ) );
		Assert( flDot != 0.0f );
		flBumpAmount = MIN( flBackMax / flDot, MAX( fabs( DotProduct( side1 - vBackMaxPos, vBumpAxis ) ), fabs( DotProduct( side2 - vBackMaxPos, vBumpAxis ) ) ) );
		flInvBumpAmount = MIN( flFrontMax / flDot, MAX( fabs( DotProduct( side1 - vFrontMaxPos, vBumpAxis ) ), fabs( DotProduct( side2 - vFrontMaxPos, vBumpAxis ) ) ) );

#if defined( GAME_DLL )
		if ( wall_debug.GetBool() )
		{
			// front side push
			NDebugOverlay::VertArrow( vFrontMaxPos - flFrontMax * vWallUp, vFrontMaxPos, 2, 255, 0, 0, 255, true, wall_debug_time.GetFloat() );
			NDebugOverlay::VertArrow( vFrontMaxPos, vFrontMaxPos - flInvBumpAmount * vBumpAxis, 2, 255, 0, 0, 255, true, wall_debug_time.GetFloat() );

			// back side push
			NDebugOverlay::VertArrow( vBackMaxPos + flBackMax * vWallUp, vBackMaxPos, 2, 0, 0, 255, 255, true, wall_debug_time.GetFloat() );
			NDebugOverlay::VertArrow( vBackMaxPos, vBackMaxPos + flBumpAmount * vBumpAxis, 2, 0, 0, 255, 255, true, wall_debug_time.GetFloat() );
		}
#endif

		// push in the negative direction of the projected normal
		if ( flFrontMax < flBackMax )
		{
			VectorNegate( vBumpAxis );
			V_swap( flBumpAmount, flInvBumpAmount );
		}
	}

	// add epsilon
	flBumpAmount += 0.1f;
	flInvBumpAmount += 0.1f;

	vNewPos += flBumpAmount * vBumpAxis;

#if defined( GAME_DLL )
	if ( wall_debug.GetBool() )
	{
		Vector vPosOffset = vNewPos - pEntity->GetAbsOrigin();
		NDebugOverlay::Sphere( ptOBBCenter + vPosOffset, 5, 0, 0, 255, true, wall_debug_time.GetFloat() );
		for ( int i=0; i<8; ++i )
		{
			NDebugOverlay::Sphere( ptOBB[i] + vPosOffset, 2, 0, 0, 255, true, wall_debug_time.GetFloat() );
		}

		NDebugOverlay::BoxAngles( pEntity->GetAbsOrigin() , vObstructionMins, vObstructionMaxs, pEntity->CollisionProp()->GetCollisionAngles(), 0, 255, 255, 64, wall_debug_time.GetFloat() );
		NDebugOverlay::BoxAngles( vNewPos , vObstructionMins, vObstructionMaxs, pEntity->CollisionProp()->GetCollisionAngles(), 255, 255, 0, 64, wall_debug_time.GetFloat() );
	}
#endif

	// check if the entity gets stuck at the new pos
	CTraceFilterSimple filter( pEntity, COLLISION_GROUP_NONE );
	trace_t stuckTrace;
	enginetrace->SweepCollideable( pEntity->GetCollideable(), vNewPos, vNewPos, vNewAngles, MASK_SOLID, &filter, &stuckTrace );

	// If safe, teleport. Otherwise, we're better off being stuck by the wall.
	if ( !stuckTrace.startsolid || bIgnoreStuck )
	{
		//EASY_DIFFPRINT( this, "CProjectedWallEntity::DisplaceObstructingEntities() teleport up" );
		// TODO: Some smoothing of the player's view or effect when this happens?

		pEntity->Teleport( &vNewPos, &vNewAngles, &vNewVel );
		return;
	}
	// the entity got stuck with horizontal bridge
	else if ( pEntity->IsPlayer() && m_bIsHorizontal )
	{
		// if success, move on
		CPortal_Player* pPlayer = ToPortalPlayer( pEntity );
		Assert ( pPlayer );
		if ( !pPlayer )
			return;

		// 1. If player wasn't crouching already, try to crouch and move player up
		if ( !pPlayer->m_Local.m_bDucked )
		{
			// If ducking stops the intersection, force them to duck
			TracePlayerBoxAgainstCollidables( stuckTrace, pPlayer, vNewPos, vNewPos, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
			if ( !stuckTrace.startsolid )
			{
				//EASY_DIFFPRINT( this, "CProjectedWallEntity::DisplaceObstructingEntities() force duck up" );
				pPlayer->ForceDuckThisFrame();
				pPlayer->Teleport( &vNewPos, &vNewAngles, &vNewVel );

#if defined( GAME_DLL )
				if ( wall_debug.GetBool() )
				{
					NDebugOverlay::BoxAngles( vNewPos , VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 64, wall_debug_time.GetFloat() );
				}
#endif

				return;
			}
		}

		// 2. If pushing up failed, try to move the player to the opposite side
		vNewPos = pEntity->GetAbsOrigin() - flInvBumpAmount * vBumpAxis;
		if ( !pPlayer->m_Local.m_bDucked )
		{
			TracePlayerBoxAgainstCollidables( stuckTrace, pPlayer, vNewPos, vNewPos, VEC_HULL_MIN, VEC_HULL_MAX );
			if ( !stuckTrace.startsolid )
			{
				//EASY_DIFFPRINT( this, "CProjectedWallEntity::DisplaceObstructingEntities() teleport down" );
				pPlayer->Teleport( &vNewPos, &vNewAngles, &vNewVel );

#if defined( GAME_DLL )
				if ( wall_debug.GetBool() )
				{
					NDebugOverlay::BoxAngles( vNewPos , VEC_HULL_MIN, VEC_HULL_MAX, pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 64, wall_debug_time.GetFloat() );
				}
#endif

				return;
			}

			// check duck
			vNewPos += 36.f * Vector( 0, 0, 1 );
			TracePlayerBoxAgainstCollidables( stuckTrace, pPlayer, vNewPos, vNewPos, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
			if ( !stuckTrace.startsolid )
			{
				//EASY_DIFFPRINT( this, "CProjectedWallEntity::DisplaceObstructingEntities() force duck down" );
				pPlayer->ForceDuckThisFrame();
				pPlayer->Teleport( &vNewPos, &vNewAngles, &vNewVel );

#if defined( GAME_DLL )
				if ( wall_debug.GetBool() )
				{
					NDebugOverlay::BoxAngles( vNewPos , VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 64, wall_debug_time.GetFloat() );
				}
#endif

				return;
			}
		}
		else
		{
			TracePlayerBoxAgainstCollidables( stuckTrace, pPlayer, vNewPos, vNewPos, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
			if ( !stuckTrace.startsolid )
			{
				//EASY_DIFFPRINT( this, "CProjectedWallEntity::DisplaceObstructingEntities() double duck down" );
				pPlayer->ForceDuckThisFrame();
				pPlayer->Teleport( &vNewPos, &vNewAngles, &vNewVel );

#if defined( GAME_DLL )
				if ( wall_debug.GetBool() )
				{
					NDebugOverlay::BoxAngles( vNewPos , VEC_HULL_MIN, VEC_HULL_MAX, pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 64, wall_debug_time.GetFloat() );
				}
#endif

				return;
			}
		}
	}
	// player stuck in not-horizontal bridge OR entity is stuck in a bridge
	else
	{
		vNewPos = pEntity->GetAbsOrigin() - flInvBumpAmount * vBumpAxis;
		UTIL_ClearTrace( stuckTrace );
		enginetrace->SweepCollideable( pEntity->GetCollideable(), vNewPos, vNewPos, vNewAngles, MASK_SOLID, &filter, &stuckTrace );

		if ( !stuckTrace.startsolid || bIgnoreStuck )
		{
			pEntity->Teleport( &vNewPos, &vNewAngles, &vNewVel );

#if defined( GAME_DLL )
			if ( wall_debug.GetBool() )
			{
				NDebugOverlay::BoxAngles( vNewPos , vObstructionMins, vObstructionMaxs, pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 64, wall_debug_time.GetFloat() );
			}

			STEAMWORKS_SELFCHECK();
#endif

			return;
		}

#if defined( GAME_DLL )
		if ( wall_debug.GetBool() )
		{
			NDebugOverlay::BoxAngles( vNewPos , vObstructionMins, vObstructionMaxs, pEntity->CollisionProp()->GetCollisionAngles(), 255, 0, 0, 64, wall_debug_time.GetFloat() );
		}
#endif
	}

	// The entity is stuck at super rare case if we get here.
	{
		AssertMsg( 0, "Rare case for the entity getting stuck with projected bridge. Investigate at CProjectedWallEntity::DisplaceObstructingEntities()." );
	}
}


void CProjectedWallEntity::GetExtents( Vector &outMins, Vector &outMaxs, float flWidthScale )
{
	// Get current orientation
	Vector vecForward, vecRight, vecUp;
#if defined( GAME_DLL )
	QAngle qAngles = GetLocalAngles();
#else
	QAngle qAngles = GetNetworkAngles();
#endif

	AngleVectors( qAngles, &vecForward, &vecRight, &vecUp );

#if defined( GAME_DLL ) && !defined( _PS3 )
	// we're assuming it's oblong, and that height is the larger
	COMPILE_TIME_ASSERT( WALL_PROJECTOR_THICKNESS > WALL_PROJECTOR_HEIGHT );
#endif

	// Set up mins/maxes to trace along
	float flHalfHeight = m_flHeight / 2.f;
	float flHalfWidth = ( m_flWidth * flWidthScale ) / 2.f;
	Vector vTmpExtent1 = ( -vecForward * FLT_EPSILON ) - ( vecUp * flHalfHeight ) - ( vecRight * flHalfWidth );
	Vector vTmpExtent2 = ( vecForward * FLT_EPSILON ) + ( vecUp * flHalfHeight ) + ( vecRight * flHalfWidth );

	// align the mins and maxs
	Vector vWallSweptBoxMins, vWallSweptBoxMaxs;
	VectorMin( vTmpExtent1, vTmpExtent2, vWallSweptBoxMins );
	VectorMax( vTmpExtent1, vTmpExtent2, vWallSweptBoxMaxs );

	outMins = vWallSweptBoxMins;
	outMaxs = vWallSweptBoxMaxs;
}
