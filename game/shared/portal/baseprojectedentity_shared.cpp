//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "baseprojectedentity_shared.h"

#if defined( GAME_DLL )
#	include "baseprojector.h"
#	include "info_placement_helper.h"
#	include "portal_gamestats.h"
#	include	"weapon_portalgun_shared.h"
#else
typedef C_BaseProjectedEntity CBaseProjectedEntity;
#include "prediction.h"
#include "c_baseprojector.h"
#endif

// offset away from the projector, so the trace doesn't start in solid
#define PROJECTEDENTITY_TRACE_OFFSET 25.f


void UTil_ProjectedEntity_Trace_Filter( CTraceFilterSimpleClassnameList *traceFilter )
{
	traceFilter->AddClassnameToIgnore( "prop_physics" );
	traceFilter->AddClassnameToIgnore( "func_physbox" );
	traceFilter->AddClassnameToIgnore( "simple_physics_brush" );
	/*traceFilter->AddClassnameToIgnore( "prop_weighted_cube" );
	traceFilter->AddClassnameToIgnore( "npc_portal_turret_floor" );
	traceFilter->AddClassnameToIgnore( "prop_energy_ball" );
	traceFilter->AddClassnameToIgnore( "npc_security_camera" );
	traceFilter->AddClassnameToIgnore( "simple_physics_prop" );
	traceFilter->AddClassnameToIgnore( "prop_ragdoll" );
	traceFilter->AddClassnameToIgnore( "prop_glados_core" );
	traceFilter->AddClassnameToIgnore( "player" );
	traceFilter->AddClassnameToIgnore( "projected_wall_entity" );
	traceFilter->AddClassnameToIgnore( "prop_paint_bomb" );
	traceFilter->AddClassnameToIgnore( "prop_exploding_futbol" );
	traceFilter->AddClassnameToIgnore( "prop_wall_projector" );
	traceFilter->AddClassnameToIgnore( "projected_wall_entity" );
	traceFilter->AddClassnameToIgnore( "projected_tractor_beam_entity" );
	traceFilter->AddClassnameToIgnore( "trigger_tractorbeam" );
	traceFilter->AddClassnameToIgnore( "physicsshadowclone" );
	traceFilter->AddClassnameToIgnore( "prop_floor_button" );*/
}


//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------
bool CBaseProjectedEntity::IsHittingPortal( Vector* pOutOrigin, QAngle* pOutAngles, CPortal_Base2D** pOutPortal )
{
#if defined( GAME_DLL )
	QAngle qAngles = GetLocalAngles();
#else
	QAngle qAngles = GetNetworkAngles();
#endif

	// Get current orientation
	Vector vForward, vecRight, vecUp;
	AngleVectors( qAngles, &vForward );

	Vector mins, maxs;
	GetProjectionExtents( mins, maxs );

#if defined( GAME_DLL )
	Vector vStart = GetLocalOrigin();
#else
	Vector vStart = GetNetworkOrigin();
#endif

	Ray_t ray;
	// Projected ents keep themselves off the walls slightly, so move this up double that ammount to make
	// sure we hit any potential portal.
	Vector rayPos = vStart + PROJECTEDENTITY_TRACE_OFFSET * vForward;// GetEndPoint() - 5.f * vForward; // back up the start pos of the ray a bit to make sure that we miss anything at the endpoint 
	ray.Init( rayPos, rayPos + ( vForward * MAX_TRACE_LENGTH ), mins, maxs );

	float flPortalTraceFraction = 1.0f;

	trace_t worldTrace;
	CTraceFilterSimpleClassnameList traceFilter( this, COLLISION_GROUP_NONE );
	UTil_ProjectedEntity_Trace_Filter( &traceFilter );
	UTIL_TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &worldTrace );
	CPortal_Base2D* pHitPortal = UTIL_Portal_FirstAlongRay( ray, flPortalTraceFraction );

	if ( pOutOrigin )
	{
		*pOutOrigin = worldTrace.endpos;
	}

	if ( pOutAngles )
	{
		*pOutAngles = qAngles;
	}

	// trace hit world brush before hitting portal
	// we need the threshold because the difference of the two fractions is very small in a valid case
	float flTraceThreshold = 0.0001f;
	if ( flPortalTraceFraction - worldTrace.fraction > flTraceThreshold )
	{
		return false;
	}

	if ( pOutPortal )
	{
		*pOutPortal = pHitPortal;
	}

	if ( !pHitPortal )
	{
		return false;
	}

	// We only care about portals that are currently linked (we don't project through anything else)
	if ( pHitPortal->IsActivedAndLinked() == false )
		return false;

	float flIntersectionFraction = UTIL_IntersectRayWithPortal( ray, pHitPortal );
	Vector vHitPoint = ray.m_Start + ray.m_Delta*flIntersectionFraction;

	// Wall hit a portal, reorient and project on the other side
	VMatrix matToLinked = pHitPortal->MatrixThisToLinked();

	Vector vNewWallOrigin;
	UTIL_Portal_PointTransform( matToLinked, vHitPoint, vNewWallOrigin );

	QAngle vNewAngles;
	UTIL_Portal_AngleTransform( matToLinked, qAngles, vNewAngles );

	Vector vNewForward;
	AngleVectors( vNewAngles, &vNewForward, NULL, NULL );

	// Move far enough in front of the portal not to be co-planar
	// (will cause traces to start solid and have z fighting issues on the renderable)
	vNewWallOrigin += vNewForward * PROJECTION_END_POINT_EPSILON;

	if ( pOutAngles )
	{
		*pOutAngles = vNewAngles;
	}

	if ( pOutOrigin )
	{
		*pOutOrigin = vNewWallOrigin;
	}

	return true;
}

#if defined( CLIENT_DLL )
ConVar cl_predict_projected_entities( "cl_predict_projected_entities", "1" );
#endif

//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------
void CBaseProjectedEntity::RecursiveProjection( bool bShouldSpawn, CBaseProjector *pParentProjector, CPortal_Base2D *pExitPortal, const Vector &vProjectOrigin, const QAngle &qProjectAngles, int iRemainingProjections, bool bDisablePlacementHelper )
{
#if defined( CLIENT_DLL )
	if( !prediction->InPrediction() || !GetPredictable() )
		return;
#endif

	AddEffects( EF_NOINTERP );

#if 0
	Vector vFlooredPosition; //HACKHACK: the inputs vary just ever so slightly from client/server. Hopefully flooring them will keep them in sync
	vFlooredPosition.x = floor( vProjectOrigin.x * 512.0f ) / 512.0f;
	vFlooredPosition.y = floor( vProjectOrigin.y * 512.0f ) / 512.0f;
	vFlooredPosition.z = floor( vProjectOrigin.z * 512.0f ) / 512.0f;
#else
	Vector vFlooredPosition = vProjectOrigin;
#endif

#if defined( GAME_DLL )
	OnPreProjected();
#endif

	Vector vOldOrigin = GetAbsOrigin();

	QAngle qModAngles; //SendProxy_Angles will perform this operation on the angles, making them differ by either extremely small values, or 360 degrees (-90 == 270 angularly, but not from a precision standpoint)
	qModAngles.x = anglemod( qProjectAngles.x );
	qModAngles.y = anglemod( qProjectAngles.y );
	qModAngles.z = anglemod( qProjectAngles.z );

#if defined( CLIENT_DLL )
	Assert( bShouldSpawn == false );
	SetNetworkOrigin( vFlooredPosition );
	SetNetworkAngles( qModAngles );
#else
	SetOwnerEntity( pParentProjector );
	SetLocalOrigin( vFlooredPosition );
	SetLocalAngles( qModAngles );
#endif
	

	//EASY_DIFFPRINT( this, "CBaseProjectedEntity::RecursiveProjection() %f %f %f\n", XYZ( vFlooredPosition ) );

	m_iMaxRemainingRecursions = iRemainingProjections;
#if defined( GAME_DLL )
	if( bShouldSpawn )
	{
		DispatchSpawn( this );
	}
	else
#endif
	{
		FindProjectedEndpoints();
	}

	if( pExitPortal )
	{
		SetSourcePortal( pExitPortal );
#if defined( GAME_DLL ) && !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
		CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( pExitPortal->m_hPlacedBy.Get() );
		if ( pPortalGun != NULL )
		{
			Vector vecForward, vecRight, vecUp;
			AngleVectors( qModAngles,  &vecForward, &vecRight, &vecUp );
			g_PortalGameStats.Event_TractorBeam_Project( pExitPortal->m_ptOrigin, vecForward , ToPortalPlayer( pPortalGun->GetOwner() ) );		
		}
#endif
	}
	OnProjected();

#if defined( CLIENT_DLL )
	if( cl_predict_projected_entities.GetBool() == false )
		return;
#endif

	// If this hits a portal, reorient through it.
	// We create a new ent to do this.
	if ( iRemainingProjections > 1 )
	{
		// If there is a portal within a small distance of our end point, reorient
		CPortal_Base2D* pHitPortal = NULL;
		Vector vNewProjectedEntityOrigin;
		QAngle qNewProjectedEntityAngles;
		bool bIsHittingPortal = IsHittingPortal( &vNewProjectedEntityOrigin, &qNewProjectedEntityAngles, &pHitPortal );
		SetHitPortal( pHitPortal );
		if ( bIsHittingPortal && pHitPortal && pHitPortal->IsActivedAndLinked() )
		{			
			CPortal_Base2D *pNewExitPortal = pHitPortal->m_hLinkedPortal.Get();
			bool bCreateNew = (m_hChildSegment.Get() == NULL);
#if defined( GAME_DLL ) //TODO: Set dormant on the client
			if( bCreateNew )
			{
				m_hChildSegment = CreateNewProjectedEntity();
			}
#else
			if( !bCreateNew )
#endif
			{
				m_hChildSegment.Get()->RecursiveProjection( bCreateNew, pParentProjector, pNewExitPortal, vNewProjectedEntityOrigin, qNewProjectedEntityAngles, iRemainingProjections - 1, bDisablePlacementHelper );
			}
		}
		// FIXME: Bring this back for DLC2
		//else if ( engine->HasPaintmap() )
		//{
		//	//TestForReflectPaint();
		//}
		else if( m_hChildSegment.Get() != NULL )
		{
#if defined( GAME_DLL ) //TODO: Set dormant on the client
			UTIL_Remove( m_hChildSegment.Get() );
			m_hChildSegment = NULL;
#endif
		}
	}

#if defined( GAME_DLL )

	if( !bDisablePlacementHelper )
	{
		m_bCreatePlacementHelper = true;
		bool bCreatePlacement = m_hPlacementHelper.Get() == NULL;
		if( bCreatePlacement )
		{
			m_hPlacementHelper = (CInfoPlacementHelper *) CreateEntityByName( "info_placement_helper" );
		}

		PlacePlacementHelper( m_hPlacementHelper );

		if( bCreatePlacement )
		{
			DispatchSpawn( m_hPlacementHelper );
		}
	}
	else
	{
		m_bCreatePlacementHelper = false;
	}

	PhysicsTouchTriggers( NULL );
#endif

	// Projected entities should probably reflect in water because they are noticeable
	AddEffects( EF_MARKED_FOR_FAST_REFLECTION );
}

//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------
void CBaseProjectedEntity::TestForProjectionChanges( void )
{
#if defined( CLIENT_DLL )
	if( cl_predict_projected_entities.GetBool() == false )
		return;
#endif
	Vector vNewPosition;
	QAngle qNewAngles;
	CPortal_Base2D* pHitPortal = NULL;
	bool bIsHittingPortal = IsHittingPortal( &vNewPosition, &qNewAngles, &pHitPortal );

	CBaseProjectedEntity *pChild = m_hChildSegment.Get();

	//if( pChild )
	//{
	//	EASY_DIFFPRINT( pChild, "CBaseProjectedEntity::TestForProjectionChanges() %i %s  %i", entindex(), bIsHittingPortal ? "true" : "false", pHitPortal ? pHitPortal->entindex() : -1 );
	//}

	// Lost the portal we were hitting: Fizzle all children
	if ( !bIsHittingPortal || (pHitPortal && !pHitPortal->IsActivedAndLinked()) )
	{
		SetHitPortal( NULL );
#if defined( GAME_DLL ) //TODO: Set dormant on the client
		float flDistSqr = GetEndPoint().DistToSqr( vNewPosition );
		if ( flDistSqr > 0.1f )
		{
			FindProjectedEndpoints();
			OnProjected();
		}
#endif

		// FIXME: Bring this back for DLC2
		// check for reflect paint
		/*if ( engine->HasPaintmap() )
		{
			TestForReflectPaint();
		}*/
#if defined( GAME_DLL )
		/*else*/ if( pChild )
		{
			UTIL_Remove( pChild );
			m_hChildSegment = NULL;
		}
#endif
	}
#if defined( GAME_DLL )
	else if( pHitPortal->IsActivedAndLinked() && (DidRedirectionPortalMove( pHitPortal ) || ((pChild == NULL) && (m_iMaxRemainingRecursions > 0))) )
#else
	else if( pHitPortal->IsActivedAndLinked() && DidRedirectionPortalMove( pHitPortal ) && (pChild != NULL) )
#endif
	{
#if defined( CLIENT_DLL )
		if( GetPredictable() )
#endif
		{
			Vector vPrevStart = GetStartPoint();
			Vector vPrevEnd = GetEndPoint();
			FindProjectedEndpoints();
			if( (vPrevStart != GetStartPoint()) || (vPrevEnd != GetEndPoint()) )
			{
				OnProjected();
			}

			SetHitPortal( pHitPortal );
		}

		//reproject child portal
		bool bCreateNew = (pChild == NULL);
#if defined( GAME_DLL )
		if( bCreateNew )
		{
			m_hChildSegment = pChild = CreateNewProjectedEntity();
		}
#else
		if( !bCreateNew && pChild->GetPredictable() )
#endif
		{
			pChild->RecursiveProjection( bCreateNew, (CBaseProjector *)GetOwnerEntity(), pHitPortal->m_hLinkedPortal.Get(), vNewPosition, qNewAngles, m_iMaxRemainingRecursions - 1, m_bCreatePlacementHelper );
		}
	}
#if defined( GAME_DLL ) //server propogates down the chain. Client evaluates each entity separately since it's not guaranteed to know about them all
	else if( pChild )
	{
		pChild->TestForProjectionChanges();
	}
#endif
}

//--------------------------------------------------------------------------------------------------
// Test for perturbation of the portal this projected entity is redirecting through
// if true, this entity's owning projector will rebuild all projected ents after this one
//--------------------------------------------------------------------------------------------------
bool CBaseProjectedEntity::DidRedirectionPortalMove( CPortal_Base2D* pPortal )
{
	if ( !pPortal )
		return true;

	// remote portal must exist to project through
	if ( pPortal->IsActivedAndLinked() == false )
		return true;

	if ( pPortal != m_hHitPortal.Get() )
		return true;

	CBaseProjectedEntity *pChild = m_hChildSegment;
	if( !pChild )
		return true;

	// close portal moved
	if ( VectorsAreEqual( pChild->m_vecSourcePortalRemoteCenter, pPortal->m_ptOrigin ) == false )
	{
		//EASY_DIFFPRINT( pChild, "CBaseProjectedEntity::DidRedirectionPortalMove() hit portal moved" );
		return true;
	}
	// close portal rotated
	if ( QAnglesAreEqual( pChild->m_vecSourcePortalRemoteAngle, pPortal->m_qAbsAngle ) == false )
	{
		return true;
	}

	//EASY_DIFFPRINT( pChild, "%f %f %f    %f %f %f", XYZ( (Vector)pChild->m_vecSourcePortalCenter ), XYZ( pPortal->m_hLinkedPortal->m_ptOrigin ) );

	// remote portal moved
	if ( VectorsAreEqual( pChild->m_vecSourcePortalCenter, pPortal->m_hLinkedPortal->m_ptOrigin ) == false )
	{
		//EASY_DIFFPRINT( pChild, "CBaseProjectedEntity::DidRedirectionPortalMove() remote portal moved" );
		return true;
	}
	// remote portal rotated
	if ( QAnglesAreEqual( pChild->m_vecSourcePortalAngle, pPortal->m_hLinkedPortal->m_qAbsAngle ) == false )
	{
		return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
// Project from origin to solid in the direction of our forward
//--------------------------------------------------------------------------------------------------
void CBaseProjectedEntity::FindProjectedEndpoints( void )
{
#if defined( GAME_DLL )
	QAngle qAngles = GetLocalAngles();
#else
	QAngle qAngles = GetNetworkAngles();
#endif

	// Get current orientation
	Vector vecForward, vecRight, vecUp;
	AngleVectors( qAngles, &vecForward, &vecRight, &vecUp );

	Vector mins, maxs;
	GetProjectionExtents( mins, maxs );

#if defined( GAME_DLL )
	Vector vStart = GetLocalOrigin();
#else
	Vector vStart = GetNetworkOrigin();
#endif

	Vector vRayPos = vStart + PROJECTEDENTITY_TRACE_OFFSET * vecForward;
	
	Ray_t ray;
	ray.Init( vRayPos, vRayPos + vecForward*PROJECTOR_MAX_LENGTH, mins, maxs );

	trace_t tr;
	CTraceFilterSimpleClassnameList traceFilter( this, COLLISION_GROUP_NONE );
	UTil_ProjectedEntity_Trace_Filter( &traceFilter );
	UTIL_TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	// Should up the max trace dist if this hits
	Assert ( tr.DidHit() );
	
	m_vecStartPoint = vStart;
	m_vecEndPoint = tr.endpos + tr.plane.normal * (PROJECTION_END_POINT_EPSILON); // Move a tiny bit off the hit surface so there's no physical overlap
}

//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------
void CBaseProjectedEntity::SetHitPortal( CPortal_Base2D* pPortal )
{
	m_hHitPortal = pPortal;
	if ( pPortal )
	{
		Assert( pPortal->IsActivedAndLinked() );
		if ( pPortal->IsActivedAndLinked() )
		{
#if defined( GAME_DLL )
			// Listen for this portal to move
			//pPortal->AddPortalEventListener( this );
#endif
		}
	}
}

CPortal_Base2D* CBaseProjectedEntity::GetHitPortal( void )
{
	return m_hHitPortal.Get();
}


//--------------------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------------------
void CBaseProjectedEntity::SetSourcePortal( CPortal_Base2D* pPortal )
{
	Assert( pPortal && pPortal->IsActivedAndLinked() );
	m_hSourcePortal.Set( pPortal );
#if defined( CLIENT_DLL )
	SetPredictionEligible( pPortal != NULL );
#endif
	if( pPortal )
	{
		m_vecSourcePortalCenter = pPortal->m_ptOrigin;
		m_vecSourcePortalRemoteCenter = pPortal->m_hLinkedPortal->m_ptOrigin;

		m_vecSourcePortalAngle = pPortal->m_qAbsAngle;
		m_vecSourcePortalRemoteAngle = pPortal->m_hLinkedPortal->m_qAbsAngle;
	}
	else
	{
		m_vecSourcePortalCenter = vec3_origin;
		m_vecSourcePortalRemoteCenter = vec3_origin;

		m_vecSourcePortalAngle = vec3_angle;
		m_vecSourcePortalRemoteAngle = vec3_angle;
	}

	if( pPortal && pPortal->GetSimulatingPlayer() )
	{
		SetPlayerSimulated( pPortal->GetSimulatingPlayer() );
	}
	else
	{
		UnsetPlayerSimulated();
	}
}

CPortal_Base2D* CBaseProjectedEntity::GetSourcePortal( void )
{
	return m_hSourcePortal.Get();
}

//--------------------------------------------------------------------------------------------------
// Specify the extents to use for the projection trace
//--------------------------------------------------------------------------------------------------
void CBaseProjectedEntity::GetProjectionExtents( Vector &outMins, Vector &outMaxs )
{
	outMins = outMaxs = vec3_origin;	
}


void CBaseProjectedEntity::OnProjected( void )
{
	AddEffects( EF_NOINTERP );
#if defined( CLIENT_DLL )
	SetNetworkOrigin( GetStartPoint() );
	PreDataChanged.vStartPoint = GetStartPoint();
	PreDataChanged.vEndPoint = GetEndPoint();
	PreDataChanged.qAngles = GetNetworkAngles();
#endif
}


void CBaseProjectedEntity::TestForReflectPaint( void )
{
	Ray_t ray;
	// make ray twice longer than the projected length, so the trace will actually hit something
	ray.Init( GetStartPoint(), GetStartPoint() + 2.f * ( GetEndPoint() - GetStartPoint() ) );
	CTraceFilterSimpleClassnameList traceFilter( this, COLLISION_GROUP_NONE );
	UTil_ProjectedEntity_Trace_Filter( &traceFilter );

	trace_t tr;
	UTIL_ClearTrace( tr );
	UTIL_TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );

	Vector vDir, vNewProjectedEntityOrigin;
	if ( UTIL_Paint_Reflect( tr, vNewProjectedEntityOrigin, vDir ) )
	{	
		// rotate psuedo up vector
		Vector vOldDir, vOldUp;
		GetVectors( &vOldDir, NULL, &vOldUp );
		Vector vRotAxis = CrossProduct( vOldDir, vDir );
		float flAngleBetween = RAD2DEG( acos( clamp( DotProduct( vOldDir, vDir ), -1.f, 1.f ) ) );
		matrix3x4_t matRotation;
		MatrixBuildRotationAboutAxis( vRotAxis, flAngleBetween, matRotation );
		Vector vNewUp;
		VectorRotate( vOldUp, matRotation, vNewUp );

		QAngle qNewProjectedEntityAngles;
		VectorAngles( vDir, vNewUp, qNewProjectedEntityAngles );

		//reproject child portal
		bool bCreateNew = (m_hChildSegment.Get() == NULL);
#if defined( GAME_DLL )
		if( bCreateNew )
		{
			m_hChildSegment = CreateNewProjectedEntity();
		}
#else
		if( !bCreateNew )
#endif
		{
			m_hChildSegment.Get()->RecursiveProjection( bCreateNew, (CBaseProjector *)GetOwnerEntity(), NULL, vNewProjectedEntityOrigin, qNewProjectedEntityAngles, m_iMaxRemainingRecursions - 1, m_bCreatePlacementHelper );
		}
	}
#if defined( GAME_DLL )
	else if ( m_hChildSegment.Get() != NULL )
	{
		UTIL_Remove( m_hChildSegment.Get() );
		m_hChildSegment = NULL;
	}
#endif
}
