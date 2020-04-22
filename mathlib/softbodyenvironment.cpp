//========= Copyright © Valve Corporation, All rights reserved. ============//
// This is analogous to RnWorld from source2, but only for softbodies
// 
#include "mathlib/softbodyenvironment.h"

//-----------------------------------------------------------------------------------------------------------------------------
CSoftbodyCollisionFilter::CSoftbodyCollisionFilter()
{
	V_memset( m_GroupPairs, 0, sizeof( m_GroupPairs ) );

	// Allow default-default shape collision by default to avoid confusion, 
	// in test worlds that won't set up their own collision filtering.
	// INTERSECTION_COLLISION_GROUP_ALWAYS is the default collision group
	InitGroup( COLLISION_GROUP_ALWAYS, INTERSECTION_PAIR_DEFAULT_COLLISION );
}


//-----------------------------------------------------------------------------------------------------------------------------
uint16 CSoftbodyCollisionFilter::TestSimulation( const RnCollisionAttr_t &left, const RnCollisionAttr_t &right )const
{
	Assert( left.m_nCollisionGroup < MAX_GROUPS && right.m_nCollisionGroup < MAX_GROUPS );

	uint16 nIntersectionFlags = 0;

	if ( left.IsSolidContactEnabled() && right.IsSolidContactEnabled() )
	{
		// Return value from a table of PxPairFlags.
		nIntersectionFlags = m_GroupPairs[ left.m_nCollisionGroup ][ right.m_nCollisionGroup ];
	}

	if ( ( ( left.m_nInteractsAs & right.m_nInteractsExclude ) | ( left.m_nInteractsExclude & right.m_nInteractsAs ) ) != 0 )
	{
		nIntersectionFlags = 0;
	}
	else if ( ( ( left.m_nInteractsAs & right.m_nInteractsWith ) | ( left.m_nInteractsWith & right.m_nInteractsAs ) ) != 0 )
	{
		nIntersectionFlags |= INTERSECTION_PAIR_TRIGGER;
	}
	else if ( left.m_nCollisionGroup == COLLISION_GROUP_CONDITIONALLY_SOLID || right.m_nCollisionGroup == COLLISION_GROUP_CONDITIONALLY_SOLID )
	{
		nIntersectionFlags = 0;
	}

	if ( !left.IsTouchEventEnabled() || !right.IsTouchEventEnabled() )
	{
		nIntersectionFlags &= ~INTERSECTION_PAIR_TRIGGER;
	}

	// no interactions within the same hierarchy
	// 0xFFFF is a special case because entindex() is -1 for all client-side-only entities
	if ( left.m_nHierarchyId != 0 && left.m_nHierarchyId != 0xFFFF && left.m_nHierarchyId == right.m_nHierarchyId )
	{
		nIntersectionFlags = 0;
	}

	return nIntersectionFlags;
}


//-----------------------------------------------------------------------------------------------------------------------------
void CSoftbodyCollisionFilter::InitGroup( int nGroup, CollisionGroupPairFlags defaultFlags )
{
	for ( int i = 0; i < MAX_GROUPS; ++i )
	{
		m_GroupPairs[ i ][ nGroup ] = m_GroupPairs[ nGroup ][ i ] = defaultFlags;
	}
	m_GroupPairs[ COLLISION_GROUP_ALWAYS ][ nGroup ] = m_GroupPairs[ nGroup ][ COLLISION_GROUP_ALWAYS ] = INTERSECTION_PAIR_DEFAULT_COLLISION;
	m_GroupPairs[ COLLISION_GROUP_TRIGGER ][ nGroup ] = m_GroupPairs[ nGroup ][ COLLISION_GROUP_TRIGGER ] = 0;
}


AABB_t CSoftbodyCollisionSphere::GetBbox()const
{
	AABB_t aabb;
	aabb.m_vMinBounds = m_vCenter - Vector( m_flRadius, m_flRadius, m_flRadius );
	aabb.m_vMaxBounds = m_vCenter + Vector( m_flRadius, m_flRadius, m_flRadius );
	return aabb;
}

AABB_t CSoftbodyCollisionCapsule::GetBbox()const
{
	AABB_t aabb;
	aabb.m_vMinBounds = VectorMin( m_vCenter[ 0 ], m_vCenter[ 1 ] ) - Vector( m_flRadius, m_flRadius, m_flRadius );
	aabb.m_vMaxBounds = VectorMax( m_vCenter[ 0 ], m_vCenter[ 1 ] ) + Vector( m_flRadius, m_flRadius, m_flRadius );
	return aabb;
}


void CSoftbodyEnvironment::Add( CSoftbodyCollisionShape * pShape )
{
	if ( pShape->GetProxyId() >= 0 )
		return; // already added

	AABB_t bbox = pShape->GetBbox();
	// bbox.Expand( 8 ); // when we add the shape for the first time, we might not know if it's going to move at all - so a good heuristic might be to NOT expand the proxy bounds at first

	int32 nProxyId = m_BroadphaseTree.CreateProxy( bbox, pShape );
	pShape->SetProxyId( nProxyId );
}


void CSoftbodyEnvironment::Update( CSoftbodyCollisionShape * pShape )
{
	int32 nProxyId = pShape->GetProxyId();
	if ( nProxyId < 0 )
		return; // proxy not added to the broadphase

	// Did the bbox move enough to warrant moving the proxy?
	AABB_t bbox = pShape->GetBbox();
	if ( !m_BroadphaseTree.GetBounds( nProxyId).Contains( bbox ) )
	{
		bbox.Expand( 8 ); // we moved the proxy... maybe a good heuristic here would be to keep track of movements and expand adaptively, depending on how much the proxy moves
		m_BroadphaseTree.MoveProxy( nProxyId, bbox );
	}
}


void CSoftbodyEnvironment::AddOrUpdate( CSoftbodyCollisionShape * pShape )
{
	int32 nProxyId = pShape->GetProxyId();
	// Did the bbox move enough to warrant moving the proxy?
	AABB_t bbox = pShape->GetBbox();
	if ( nProxyId < 0 )
	{
		int32 nProxyId = m_BroadphaseTree.CreateProxy( bbox, pShape );
		pShape->SetProxyId( nProxyId );
	}
	else if ( !m_BroadphaseTree.GetBounds( nProxyId ).Contains( bbox ) )
	{
		bbox.Expand( 8 ); // we moved the proxy... maybe a good heuristic here would be to keep track of movements and expand adaptively, depending on how much the proxy moves
		m_BroadphaseTree.MoveProxy( nProxyId, bbox );
	}
}

void CSoftbodyEnvironment::Remove( CSoftbodyCollisionShape * pShape )
{
	int nProxyId = pShape->GetProxyId();
	if ( nProxyId >= 0 )
	{
		m_BroadphaseTree.DestroyProxy( pShape->GetProxyId() );
		pShape->SetProxyId( -1 );
	}
}

void CSoftbodyEnvironment::SetWind( const Vector & vWind )
{
	float flStrength = vWind.Length();
	if ( fabsf( m_vWindDesc.w ) < FLT_EPSILON )
	{
		SetNoWind();
	}
	else
	{
		m_vWindDesc.Init( vWind / flStrength, flStrength );
	}
}
