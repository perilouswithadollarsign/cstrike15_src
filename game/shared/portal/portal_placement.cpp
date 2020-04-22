//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "portal_placement.h"
#include "portal_shareddefs.h"
#include "prop_portal_shared.h"
#include "collisionutils.h"
#include "decals.h"
#include "debugoverlay_shared.h"
#include "portal_mp_gamerules.h"

#if defined( GAME_DLL )
#include "func_noportal_volume.h"
#include "triggers.h"
#include "func_portal_bumper.h"
#include "physicsshadowclone.h"
#include "trigger_portal_cleanser.h"
#else
#include "c_triggers.h"
#include "c_func_noportal_volume.h"
#include "c_func_portal_bumper.h"
#include "c_trigger_portal_cleanser.h"
#endif

#include "cegclientwrapper.h"


#include "paint_color_manager.h"
ConVar sv_portal_placement_on_paint("sv_portal_placement_on_paint", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Enable/Disable placing portal on painted surfaces");

ConVar sv_portal_placement_never_fail("sv_portal_placement_never_fail", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
extern ConVar sv_allow_mobile_portals;

#define MAXIMUM_BUMP_DISTANCE ( ( fHalfWidth * 2.0f ) * ( fHalfWidth * 2.0f ) + ( fHalfHeight * 2.0f ) * ( fHalfHeight * 2.0f ) ) / 2.0f


struct CPortalCornerFitData
{
	trace_t trCornerTrace;
	Vector ptIntersectionPoint;
	Vector vIntersectionDirection;
	Vector vBumpDirection;
	bool bCornerIntersection;
	bool bSoftBump;
};


CUtlVector<CBaseEntity *> g_FuncBumpingEntityList;
bool g_bBumpedByLinkedPortal;

ConVar sv_portal_placement_debug("sv_portal_placement_debug", "0", FCVAR_REPLICATED );
ConVar sv_portal_placement_never_bump("sv_portal_placement_never_bump", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

bool IsMaterialInList( const csurface_t &surface, char *g_ppszMaterials[] )
{
	char szLowerName[ 256 ];
	Q_strcpy( szLowerName, surface.name );
	Q_strlower( szLowerName );

	int iMaterial = 0;

	while ( g_ppszMaterials[ iMaterial ] )
	{
		if ( Q_strstr( szLowerName, g_ppszMaterials[ iMaterial ] ) )
			return true;

		++iMaterial;
	}

	return false;
}

// exposed here as non-constant so CEG can populate the value at DLL init time
static int CEG_PORTAL_POWER = 0xffffffff; // no paint power until correctly initialized
CEG_NOINLINE void InitPortalPaintPowerValue()
{
	CEG_GCV_PRE();
	CEG_PORTAL_POWER = CEG_GET_CONSTANT_VALUE( PaintPortalPower );
	CEG_GCV_POST();
}

CEG_NOINLINE bool IsOnPortalPaint( const trace_t &tr )
{
	if ( sv_portal_placement_on_paint.GetBool() && tr.m_pEnt)
	{
		if ( !UTIL_IsPaintableSurface( tr.surface ) )
			return false;

		PaintPowerType paintPower = NO_POWER;

		//Trace for paint on the surface if it is the world
		if( tr.m_pEnt->IsBSPModel() )
		{
			// enable portal placement on painted surface
			paintPower = UTIL_Paint_TracePower( tr.m_pEnt, tr.endpos, tr.plane.normal );
		}
		else //For entities
		{
			if( FClassnameIs( tr.m_pEnt, "func_brush" ) )
			{
				paintPower = MapColorToPower( tr.m_pEnt->GetRenderColor() );
			}
		}

		if( paintPower == CEG_PORTAL_POWER )
		{
			return true;
		}
	}

	return false;
}

// exposed here as non-constant so CEG can populate the value at DLL init time
static unsigned short CEG_SURF_NO_PORTAL_FLAG = 0xffff; // portals can't be placed until correctly initialized
CEG_NOINLINE void InitSurfNoPortalFlag()
{
	CEG_GCV_PRE();
	CEG_SURF_NO_PORTAL_FLAG = CEG_GET_CONSTANT_VALUE( SurfNoPortalFlag );
	CEG_GCV_POST();
}

CEG_NOINLINE PortalSurfaceType_t PortalSurfaceType( const trace_t& tr )
{
	//Note: this is for placing portal on paint
	if ( IsOnPortalPaint( tr ) )
		return PORTAL_SURFACE_PAINT;

	if ( tr.surface.flags & CEG_SURF_NO_PORTAL_FLAG )
		return PORTAL_SURFACE_INVALID;

	const surfacedata_t *pdata = physprops->GetSurfaceData( tr.surface.surfaceProps );
	if ( pdata->game.material == CHAR_TEX_GLASS )
		return PORTAL_SURFACE_INVALID;

	// Skipping all studio models
	if ( StringHasPrefix( tr.surface.name, "**studio**" ) )
		return PORTAL_SURFACE_INVALID;

	return PORTAL_SURFACE_VALID;
}


bool IsNoPortalMaterial( const trace_t &tr )
{
	return PortalSurfaceType( tr ) == PORTAL_SURFACE_INVALID;
}

bool IsPassThroughMaterial( const csurface_t &surface )
{
	if ( surface.flags & SURF_SKY )
		return true;

	if ( IsMaterialInList( surface, g_ppszPortalPassThroughMaterials ) )
		return true;

	return false;
}


void TracePortals( const CProp_Portal *pIgnorePortal, const Vector &vForward, const Vector &vStart, const Vector &vEnd, trace_t &tr )
{
	UTIL_ClearTrace( tr );

	Ray_t ray;
	ray.Init( vStart, vEnd );

	trace_t trTemp;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal != pIgnorePortal && pTempPortal->IsActive() )
			{
				Vector vOtherOrigin = pTempPortal->GetAbsOrigin();
				QAngle qOtherAngles = pTempPortal->GetAbsAngles();

				Vector vLinkedForward;
				AngleVectors( qOtherAngles, &vLinkedForward, NULL, NULL );

				// If they're not on the same face then don't worry about overlap
				if ( vForward.Dot( vLinkedForward ) < 0.95f )
					continue;

				UTIL_IntersectRayWithPortalOBBAsAABB( pTempPortal, ray, &trTemp );

				if ( trTemp.fraction < 1.0f && trTemp.fraction < tr.fraction )
				{
					tr = trTemp;
					tr.m_pEnt = pTempPortal;
				}
			}
		}
	}
}

int AllEdictsAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );
#if defined( GAME_DLL )
	partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, &rayEnum );
#else
	partition->EnumerateElementsAlongRay( PARTITION_ALL_CLIENT_EDICTS, ray, false, &rayEnum );
#endif
	return rayEnum.GetCount();
}

bool TraceBumpingEntities( const Vector &vStart, const Vector &vEnd, trace_t &tr )
{
	UTIL_ClearTrace( tr );

	// We use this so portal bumpers can't squeeze a portal into not fitting
	bool bClosestIsSoftBumper = false;

	// Trace to the surface to see if there's a rotating door in the way
	CBaseEntity *list[1024];

	Ray_t ray;
	ray.Init( vStart, vEnd );

	int nCount = AllEdictsAlongRay( list, 1024, ray, 0 );

	for ( int i = 0; i < nCount; i++ )
	{
#if 0
#if defined( GAME_DLL )
		Warning( "TraceBumpingEntities(server) : %s\n", list[i]->m_iClassname );
#else
		Warning( "TraceBumpingEntities(client) : %s\n", list[i]->GetClassname() );
#endif
#endif

		trace_t trTemp;
		UTIL_ClearTrace( trTemp );

		bool bSoftBumper = false;

		if ( dynamic_cast<CFuncPortalBumper*>( list[i] ) != NULL )
		{
			if( ((CFuncPortalBumper *)list[i])->IsActive() )
			{
				bSoftBumper = true;
				enginetrace->ClipRayToEntity( ray, MASK_ALL, list[i], &trTemp );
				if ( trTemp.startsolid )
				{
					trTemp.fraction = 1.0f;
				}
			}
		}
		else if ( dynamic_cast<CTriggerPortalCleanser*>( list[i] ) != NULL )
		{
			if( ((CTriggerPortalCleanser *)list[i])->IsEnabled() )
			{
				enginetrace->ClipRayToEntity( ray, MASK_ALL, list[i], &trTemp );
				if ( trTemp.startsolid )
				{
					trTemp.fraction = 1.0f;
				}
			}
		}
		else if ( dynamic_cast<CFuncNoPortalVolume*>( list[i] ) != NULL )
		{
			trTemp.fraction = 1.0f;
			if ( static_cast<CFuncNoPortalVolume*>( list[i] )->IsActive() )
			{
				enginetrace->ClipRayToEntity( ray, MASK_ALL, list[i], &trTemp );

				// Bump by an extra 2 units so that the portal isn't touching the no portal volume
				Vector vDelta = trTemp.endpos - trTemp.startpos;
				float fLength = VectorNormalize( vDelta ) - 2.0f;
				if ( fLength < 0.0f )
					fLength = 0.0f;
				trTemp.fraction = fLength / ray.m_Delta.Length();
				trTemp.endpos = trTemp.startpos + vDelta * fLength;
			}
		}
#if defined( GAME_DLL )
		else if ( FClassnameIs( list[i], "prop_door_rotating" ) )
#else
		else if ( FClassnameIs( list[i], "class C_PropDoorRotating" ) )
#endif	
		{

			// Check more precise door collision
			//CBasePropDoor *pRotatingDoor = static_cast<CBasePropDoor *>( list[i] );
			CBaseEntity *pRotatingDoor = list[i];

			pRotatingDoor->TestCollision( ray, 0, trTemp );
		}

		// If this is the closest and has only bumped once (for soft bumpers)
		if ( trTemp.fraction < tr.fraction && ( !bSoftBumper || !g_FuncBumpingEntityList.HasElement( list[i] ) ) )
		{
			tr = trTemp;
			bClosestIsSoftBumper = bSoftBumper;
		}
	}

	return bClosestIsSoftBumper;
}

bool TracePortalCorner( const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const Vector &vCorner, const Vector &vForward, PortalPlacedBy_t ePlacedBy, ITraceFilter *pTraceFilterPortalShot, trace_t &tr, bool &bSoftBump )
{
	Vector vOriginToCorner = vCorner - vOrigin;

	// Check for surface edge
	trace_t trSurfaceEdge;
	UTIL_TraceLine( vOrigin - vForward, vCorner - vForward, MASK_SHOT_PORTAL|CONTENTS_WATER|CONTENTS_SLIME, pTraceFilterPortalShot, &trSurfaceEdge );

	if ( trSurfaceEdge.startsolid )
	{
		float fTotalFraction = trSurfaceEdge.fractionleftsolid;

		while ( trSurfaceEdge.startsolid && trSurfaceEdge.fractionleftsolid > 0.0f && fTotalFraction < 1.0f )
		{
			UTIL_TraceLine( vOrigin + vOriginToCorner * ( fTotalFraction + 0.05f ) - vForward, vCorner + vOriginToCorner * ( fTotalFraction + 0.05f ) - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &trSurfaceEdge );

			if ( trSurfaceEdge.startsolid )
			{
				fTotalFraction += trSurfaceEdge.fractionleftsolid + 0.05f;
			}
		}

		if ( fTotalFraction < 1.0f )
		{
			UTIL_TraceLine( vOrigin + vOriginToCorner * ( fTotalFraction + 0.05f ) - vForward, vOrigin - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &trSurfaceEdge );

			if ( trSurfaceEdge.startsolid )
			{
				trSurfaceEdge.fraction = 1.0f;
			}
			else
			{
				trSurfaceEdge.fraction = fTotalFraction;
				trSurfaceEdge.plane.normal = -trSurfaceEdge.plane.normal;
			}
		}
		else
		{
			trSurfaceEdge.fraction = 1.0f;
		}
	}
	else
	{
		trSurfaceEdge.fraction = 1.0f;
	}

	// Check for enclosing wall
	trace_t trEnclosingWall;
	UTIL_TraceLine( vOrigin + vForward, vCorner + vForward, MASK_SOLID_BRUSHONLY|CONTENTS_MONSTER|CONTENTS_WATER|CONTENTS_SLIME, pTraceFilterPortalShot, &trEnclosingWall );

	if ( trSurfaceEdge.fraction < trEnclosingWall.fraction )
	{
		trEnclosingWall.fraction = trSurfaceEdge.fraction;
		trEnclosingWall.plane.normal = trSurfaceEdge.plane.normal;
	}

	trace_t trPortal;
	trace_t trBumpingEntity;

	if ( ePlacedBy != PORTAL_PLACED_BY_FIXED )
		TracePortals( pIgnorePortal, vForward, vOrigin + vForward, vCorner + vForward, trPortal );
	else
		UTIL_ClearTrace( trPortal );

	bool bSoftBumper = TraceBumpingEntities( vOrigin + vForward, vCorner + vForward, trBumpingEntity );

	if ( trEnclosingWall.fraction >= 1.0f && trPortal.fraction >= 1.0f && trBumpingEntity.fraction >= 1.0f )
	{
		//check for a surface change between center and corner so we can bump when we partially overlap a non-portal surface		
		trace_t cornerTrace;
		UTIL_TraceLine( vCorner, vCorner - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &cornerTrace );
		if( cornerTrace.DidHit() && IsNoPortalMaterial( cornerTrace ) )
		{
			//a bump is in order, try to determine where we transition to the no portal material with a binary search
			float fFullLength = vOriginToCorner.Length();
			float fBadLength = fFullLength;
			float fGoodLength = 0.0f;
			Vector vOriginToCornerNormalized = vOriginToCorner.Normalized();
			int iSearchCount = 0; //overwatch is soon. And I think this loop might have locked up once or twice without an absolute loop limit. Being safe for now.

			const float kMaxDelta = 0.01f;

			while( ((fBadLength - fGoodLength) >= kMaxDelta) && (iSearchCount < 100) )
			{
				AssertOnce( iSearchCount < 50 );
				float fTestLength = (fBadLength + fGoodLength) * 0.5f;
				Vector vTestSpot = vOrigin + (vOriginToCornerNormalized * fTestLength);
				UTIL_TraceLine( vTestSpot, vTestSpot - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &cornerTrace );
				if( cornerTrace.DidHit() && !IsNoPortalMaterial( cornerTrace ) )
				{
					fGoodLength = fTestLength;
				}
				else
				{
					fBadLength = fTestLength;
				}
				++iSearchCount;
			}

			Vector vGoodSpot = vOrigin + (vOriginToCornerNormalized * fGoodLength);
			Vector vBadSpot = vOrigin + (vOriginToCornerNormalized * fBadLength);

			iSearchCount = 0;
			Vector vImpactNormal( 0.0f, 0.0f, 0.0f );
			//try spots at 4x the delta in a circular pattern to find the normal of impact
			{
				Vector vGoodDirection = vForward.Cross( vOriginToCornerNormalized );
				Vector vTestSpot = vGoodSpot + (vGoodDirection * (kMaxDelta * 4.0f));
				UTIL_TraceLine( vTestSpot, vTestSpot - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &cornerTrace );
				if( !(cornerTrace.DidHit() && !IsNoPortalMaterial( cornerTrace )) )
				{
					vGoodDirection = -vGoodDirection;
				}

				vTestSpot = vGoodSpot + (vGoodDirection * (kMaxDelta * 4.0f));
				UTIL_TraceLine( vTestSpot, vTestSpot - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &cornerTrace );
				if( cornerTrace.DidHit() && !IsNoPortalMaterial( cornerTrace ) )
				{
					float fBadAngle = 0.0f;
					float fGoodAngle = 90.0f; 
					for( int i = 0; i != 10; ++i ) //we'd like a delta of less than 0.1 degrees. And the deltas are predictable. 90, 45, 22.5, 11.25, 5.625, 2.8125, 1.40625, 0.703125, 0.3515625, 0.17578125, 0.087890625
					{
						float fTestAngle = (fBadAngle + fGoodAngle) * 0.5f;
						Vector vTestDirection = (cosf( fTestAngle ) * vOriginToCornerNormalized) + (sinf( fTestAngle ) * vGoodDirection);
						vTestSpot = vGoodSpot + (vTestDirection * (kMaxDelta * 4.0f));
						UTIL_TraceLine( vTestSpot, vTestSpot - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &cornerTrace );
						if( cornerTrace.DidHit() && !IsNoPortalMaterial( cornerTrace ) )
						{
							fGoodAngle = fTestAngle;
						}
						else
						{
							fBadAngle = fTestAngle;
						}
					}

					vGoodDirection = (cosf( fGoodAngle ) * vOriginToCornerNormalized) + (sinf( fGoodAngle ) * vGoodDirection);
					vImpactNormal = vForward.Cross( vGoodDirection );
					if( vImpactNormal.Dot( vOriginToCornerNormalized ) > 0.0f )
						vImpactNormal = -vImpactNormal;
				}				
			}
			
			tr = cornerTrace;
			tr.startpos = vOrigin;
			tr.endpos = vOrigin + (vOriginToCornerNormalized * fGoodLength);
			tr.fraction = fGoodLength / fFullLength;
			tr.fractionleftsolid = 1.0f;
			tr.plane.normal = vImpactNormal; //.Init();// = -vOriginToCorner.Normalized();
			tr.plane.dist = tr.plane.normal.Dot( tr.endpos );

			return true;
		}


		UTIL_ClearTrace( tr );
		return false;
	}

	if ( trEnclosingWall.fraction <= trPortal.fraction && trEnclosingWall.fraction <= trBumpingEntity.fraction )
	{
		tr = trEnclosingWall;
		bSoftBump = false;
	}
	else if ( trPortal.fraction <= trEnclosingWall.fraction && trPortal.fraction <= trBumpingEntity.fraction && !g_FuncBumpingEntityList.HasElement( trPortal.m_pEnt ) )
	{
		tr = trPortal;

		CProp_Portal *pBumpPortal = static_cast< CProp_Portal * >( trPortal.m_pEnt );
		bool bBumpedByOwnPortal = pBumpPortal->GetFiredByPlayer() == pIgnorePortal->GetFiredByPlayer();
		if ( bBumpedByOwnPortal )
		{
			g_bBumpedByLinkedPortal = true;			
		}

		bSoftBump = !bBumpedByOwnPortal;
	}
	else if ( !trBumpingEntity.startsolid && trBumpingEntity.fraction <= trEnclosingWall.fraction && trBumpingEntity.fraction <= trPortal.fraction )
	{
		tr = trBumpingEntity;
		bSoftBump = bSoftBumper;
	}
	else
	{
		UTIL_ClearTrace( tr );
		return false;
	}

	return true;
}

Vector FindBumpVectorInCorner( const Vector &ptCorner1, const Vector &ptCorner2, const Vector &ptIntersectionPoint1, const Vector &ptIntersectionPoint2, const Vector &vIntersectionDirection1, const Vector &vIntersectionDirection2, const Vector &vIntersectionBumpDirection1, const Vector &vIntersectionBumpDirection2 )
{
	Vector ptClosestSegment1, ptClosestSegment2;
	float fT1, fT2;

	CalcLineToLineIntersectionSegment( ptIntersectionPoint1, ptIntersectionPoint1 + vIntersectionDirection1, 
		ptIntersectionPoint2, ptIntersectionPoint2 + vIntersectionDirection2,
		&ptClosestSegment1, &ptClosestSegment2, &fT1, &fT2 );

	Vector ptLineIntersection = ( ptClosestSegment1 + ptClosestSegment2 ) * 0.5f;

	// The 2 corner trace intersections and the intersection of those lines makes a triangle.
	// We want to make a similar triangle where the base is large enough to fit the edge of the portal

	// Get the the small triangle's legs and leg lengths
	Vector vShortLeg = ptIntersectionPoint1 - ptLineIntersection;
	Vector vShortLeg2 = ptIntersectionPoint2 - ptLineIntersection;

	float fShortLegLength = vShortLeg.Length();
	float fShortLeg2Length = vShortLeg2.Length();

	if ( fShortLegLength == 0.0f || fShortLeg2Length == 0.0f )
	{
		// FIXME: Our triangle is actually a point or a line, so there's nothing we can do
		return vec3_origin;
	}

	// Normalized legs
	vShortLeg /= fShortLegLength;
	vShortLeg2 /= fShortLeg2Length;

	// Check if corners are aligned with one of the legs
	Vector vCornerToCornerNorm = ptCorner2 - ptCorner1;
	VectorNormalize( vCornerToCornerNorm );

	float fPortalEdgeDotLeg = vCornerToCornerNorm.Dot( vShortLeg );
	float fPortalEdgeDotLeg2 = vCornerToCornerNorm.Dot( vShortLeg2 );

	if ( fPortalEdgeDotLeg < -0.9999f || fPortalEdgeDotLeg > 0.9999f || fPortalEdgeDotLeg2 < -0.9999f || fPortalEdgeDotLeg2 > 0.9999f )
	{
		// Do a one corner bump with corner 1
		float fBumpDistance1 = CalcDistanceToLine( ptCorner1, ptIntersectionPoint1, ptIntersectionPoint1 + vIntersectionDirection1 );

		fBumpDistance1 += PORTAL_BUMP_FORGIVENESS;

		// Do a one corner bump with corner 2
		float fBumpDistance2 = CalcDistanceToLine( ptCorner2, ptIntersectionPoint2, ptIntersectionPoint2 + vIntersectionDirection2 );

		fBumpDistance2 += PORTAL_BUMP_FORGIVENESS;

		return vIntersectionBumpDirection1 * fBumpDistance1 + vIntersectionBumpDirection2 * fBumpDistance2;
	}

	float fLegsDot = vShortLeg.Dot( vShortLeg2 );

	// Need to know if the triangle is pointing toward the portal or away from the portal
	/*bool bPointingTowardPortal = true;

	Vector vLineIntersectionToCornerNorm = ptCorner1 - ptLineIntersection;
	VectorNormalize( vLineIntersectionToCornerNorm );

	if ( vLineIntersectionToCornerNorm.Dot( vShortLeg2 ) < fLegsDot )
	{
	bPointingTowardPortal = false;
	}

	if ( !bPointingTowardPortal )*/
	{
		// Get the small triangle's base length
		float fLongBaseLength = ptCorner1.DistTo( ptCorner2 );

		// Get the large triangle's base length
		float fShortLeg2Angle = acosf( vCornerToCornerNorm.Dot( -vShortLeg ) );
		float fShortBaseAngle = acosf( fLegsDot );
		float fShortLegAngle = M_PI_F - fShortBaseAngle - fShortLeg2Angle;

		if ( sinf( fShortLegAngle ) == 0.0f )
		{
			return Vector( 1000.0f, 1000.0f, 1000.0f );
		}

		float fShortBaseLength = sinf( fShortBaseAngle ) * ( fShortLegLength / sinf( fShortLegAngle ) );

		// Avoid divide by zero
		if ( fShortBaseLength == 0.0f )
		{
			return Vector( 0.0f, 0.0f, 0.0f );
		}

		// Use ratio to get the big triangles leg length
		float fLongLegLength = fLongBaseLength * ( fShortLegLength / fShortBaseLength );

		// Get the relative point on the large triangle
		Vector ptNewCornerPos = ptLineIntersection + vShortLeg * fLongLegLength;

		// Bump by the same amount the corner has to move to fit
		return ptNewCornerPos - ptCorner1;
	}
	/*else
	{
	return Vector( 0.0f, 0.0f, 0.0f );
	}*/
}


bool FitPortalOnSurface( const CProp_Portal *pIgnorePortal, Vector &vOrigin, const Vector &vForward, const Vector &vRight, 
						 const Vector &vTopEdge, const Vector &vBottomEdge, const Vector &vRightEdge, const Vector &vLeftEdge, 
						 PortalPlacedBy_t ePlacedBy, ITraceFilter *pTraceFilterPortalShot, 
						 float fHalfWidth, float fHalfHeight,
						 int iRecursions /*= 0*/, const CPortalCornerFitData *pPortalCornerFitData /*= 0*/, const int *p_piIntersectionIndex /*= 0*/, const int *piIntersectionCount /*= 0*/ )
{
	// Don't infinitely recurse
	if ( iRecursions >= 6 )
	{
		return false;
	}

	Vector pptCorner[ 4 ];

	// Get corner points
	pptCorner[ 0 ] = vOrigin + vTopEdge + vLeftEdge;
	pptCorner[ 1 ] = vOrigin + vTopEdge + vRightEdge;
	pptCorner[ 2 ] = vOrigin + vBottomEdge + vLeftEdge;
	pptCorner[ 3 ] = vOrigin + vBottomEdge + vRightEdge;

	// Corner data
	CPortalCornerFitData sFitData[ 4 ];
	int piIntersectionIndex[ 4 ];
	int iIntersectionCount = 0;

	// Gather data we already know
	if ( pPortalCornerFitData )
	{
		for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
		{
			sFitData[ iIntersection ] = pPortalCornerFitData[ iIntersection ];
		}
	}
	else
	{
		memset( sFitData, 0, sizeof( sFitData ) );
	}

	if ( p_piIntersectionIndex )
	{
		for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
		{
			piIntersectionIndex[ iIntersection ] = p_piIntersectionIndex[ iIntersection ];
		}
	}
	else
	{
		memset( piIntersectionIndex, 0, sizeof( piIntersectionIndex ) );
	}

	if ( piIntersectionCount )
	{
		iIntersectionCount = *piIntersectionCount;
	}

	int iOldIntersectionCount = iIntersectionCount;

	Vector vNoNormalMin( 0, 0, 0 ), vNoNormalMax( 0, 0, 0 ), vNoNormalAdditive( 0, 0, 0 );
	bool bNewIntersection[4] = { false };

	// Find intersections from center to each corner
	for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
	{
		// HACK: In weird cases intersection count can go over 3 and index outside of our arrays. Don't let this happen!
		if ( iIntersectionCount < 4 )
		{
			// Don't recompute intersection data that we already have
			if ( !sFitData[ iIntersection ].bCornerIntersection )
			{
				// Test intersection of the current corner
				sFitData[ iIntersection ].bCornerIntersection = TracePortalCorner( pIgnorePortal, vOrigin, pptCorner[ iIntersection ], vForward, ePlacedBy, pTraceFilterPortalShot, sFitData[ iIntersection ].trCornerTrace, sFitData[ iIntersection ].bSoftBump );

				// If it intersected
				if ( sFitData[ iIntersection ].bCornerIntersection )
				{
					sFitData[ iIntersection ].ptIntersectionPoint = vOrigin + ( pptCorner[ iIntersection ] - vOrigin ) * sFitData[ iIntersection ].trCornerTrace.fraction;
					if ( sFitData[ iIntersection ].trCornerTrace.plane.normal.IsZero() )
					{
						Vector vPush = sFitData[ iIntersection ].ptIntersectionPoint - pptCorner[ iIntersection ];
						vNoNormalMin = vNoNormalMin.Min( vPush );
						vNoNormalMax = vNoNormalMax.Max( vPush );
						vNoNormalAdditive += vPush;
					}

					bNewIntersection[iIntersection] = true;
					piIntersectionIndex[ iIntersectionCount ] = iIntersection;
					++iIntersectionCount;
				}
			}
			else
			{
				// We shouldn't be intersecting with any old corners
				sFitData[ iIntersection ].trCornerTrace.fraction = 1.0f;
			}
		}
	}

	//clip the additive vector of pushes from intersections with no normal. We're going to give them all a shared normal that agrees
	vNoNormalAdditive = vNoNormalAdditive.Min( vNoNormalMax );
	vNoNormalAdditive = vNoNormalAdditive.Max( vNoNormalMin );
	vNoNormalAdditive.NormalizeInPlace();

	for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
	{
		if ( bNewIntersection[iIntersection] )
		{
			if ( sFitData[ iIntersection ].trCornerTrace.plane.normal.IsZero() )
			{
				sFitData[ iIntersection ].trCornerTrace.plane.normal = vNoNormalAdditive;
				sFitData[ iIntersection ].trCornerTrace.plane.dist = vNoNormalAdditive.Dot( sFitData[ iIntersection ].ptIntersectionPoint );
			}
			VectorNormalize( sFitData[ iIntersection ].trCornerTrace.plane.normal );
			sFitData[ iIntersection ].vIntersectionDirection = sFitData[ iIntersection ].trCornerTrace.plane.normal.Cross( vForward );
			VectorNormalize( sFitData[ iIntersection ].vIntersectionDirection );
			sFitData[ iIntersection ].vBumpDirection = vForward.Cross( sFitData[ iIntersection ].vIntersectionDirection );
			VectorNormalize( sFitData[ iIntersection ].vBumpDirection );

			if ( sv_portal_placement_debug.GetBool() )
			{
				for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
				{
					NDebugOverlay::Line( sFitData[ iIntersection ].ptIntersectionPoint - sFitData[ iIntersection ].vIntersectionDirection * 32.0f, 
						sFitData[ iIntersection ].ptIntersectionPoint + sFitData[ iIntersection ].vIntersectionDirection * 32.0f, 
						0, 0, 255, true, 0.5f );
				}
			}
		}
	}

	for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
	{
		// Remember soft bumpers so we don't bump with it twice
		if ( sFitData[ iIntersection ].bSoftBump )
		{
			g_FuncBumpingEntityList.AddToTail( sFitData[ iIntersection ].trCornerTrace.m_pEnt );
		}
	}

	// If no new intersections were found then it already fits
	if ( iOldIntersectionCount == iIntersectionCount )
	{
		return true;
	}

	switch ( iIntersectionCount )
	{
	case 0:
		{
			// If no corners intersect it already fits
			return true;
		}
		break;

	case 1:
		{
			float fBumpDistance = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ 0 ] ], 
				sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint, 
				sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection );

			fBumpDistance += PORTAL_BUMP_FORGIVENESS;

			vOrigin += sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection * fBumpDistance;

			return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
		}
		break;

	case 2:
		{
			if ( sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint == sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint )
			{
				return false;
			}

			float fDot = sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection );

			// If there are parallel intersections try scooting it away from a near wall
			if ( fDot < -0.9f )
			{
				// Check if perpendicular wall is near
				trace_t trPerpWall1;
				bool bSoftBump1;
				bool bDir1 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * fHalfWidth * 2.0f, vForward, ePlacedBy, pTraceFilterPortalShot, trPerpWall1, bSoftBump1 );

				trace_t trPerpWall2;
				bool bSoftBump2;
				bool bDir2 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * -fHalfWidth * 2.0f, vForward, ePlacedBy, pTraceFilterPortalShot, trPerpWall2, bSoftBump2 );

				// No fit if there's blocking walls on both sides it can't fit
				if ( bDir1 && bDir2 )
				{
					if ( bSoftBump1 )
						bDir1 = false;
					else if ( bSoftBump2 )
						bDir1 = true;
					else
						return false;
				}

				// If there's no assumption to make, just pick a direction.
				if ( !bDir1 && !bDir2 )
				{
					bDir1 = true;
				}

				// Bump the portal
				if ( bDir1 )
				{
					vOrigin += sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * -fHalfWidth;
				}
				else
				{
					vOrigin += sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * fHalfWidth;
				}

				// Prepare data for recursion
				iIntersectionCount = 0;
				sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;

				return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );	
			}

			// If they are the same there's an easy way
			if ( fDot > 0.9f )
			{
				// Get the closest intersection to the portal's center
				int iClosestIntersection = ( ( vOrigin.DistTo( sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint ) < vOrigin.DistTo( sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint ) ) ? ( 0 ) : ( 1 ) );

				// Find the largest amount that the portal needs to bump for the corner to pass the intersection
				float pfBumpDistance[ 2 ];

				for ( int iIntersection = 0; iIntersection < 2; ++iIntersection )
				{
					pfBumpDistance[ iIntersection ] = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIntersection ] ], 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vIntersectionDirection );

					pfBumpDistance[	iIntersection ] += PORTAL_BUMP_FORGIVENESS;
				}

				int iLargestBump = ( ( pfBumpDistance[ 0 ] > pfBumpDistance[ 1 ] ) ? ( 0 ) : ( 1 ) );

				// Bump the portal
				vOrigin += sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vBumpDirection * pfBumpDistance[ iLargestBump ];

				// If they were parallel to the intersection line don't invalidate both before recursion
				if ( pfBumpDistance[ 0 ] == pfBumpDistance[ 1 ] )
				{
					sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
					iIntersectionCount = 0;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
				else
				{
					// Prepare data for recursion
					if ( iLargestBump != iClosestIntersection )
					{
						sFitData[ piIntersectionIndex[ iLargestBump ] ] = sFitData[ piIntersectionIndex[ iClosestIntersection ] ];
					}
					sFitData[ piIntersectionIndex[ ( ( iLargestBump == 0 ) ? ( 1 ) : ( 0 ) ) ] ].bCornerIntersection = false;
					piIntersectionIndex[ 0 ] = piIntersectionIndex[ iLargestBump ];
					iIntersectionCount = 1;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
			}

			// Intersections are angled, bump based on math using the corner
			vOrigin += FindBumpVectorInCorner( pptCorner[ piIntersectionIndex[ 0 ] ], pptCorner[ piIntersectionIndex[ 1 ] ], 
				sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint, sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint,
				sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection, sFitData[ piIntersectionIndex[ 1 ] ].vIntersectionDirection,
				sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection, sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection );

			return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
		}
		break;

	case 3:
		{
			// Get the relationships of the intersections
			float fDot[ 3 ];
			fDot[ 0 ] = sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection );
			fDot[ 1 ] = sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 2 ] ].vBumpDirection );
			fDot[ 2 ] = sFitData[ piIntersectionIndex[ 2 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection );

			int iSimilarWalls = 0;

			for ( int iDot = 0; iDot < 3; ++iDot )
			{
				// If there are parallel intersections try scooting it away from a near wall
				if ( fDot[ iDot ] < -0.99f )
				{
					// Check if perpendicular wall is near
					trace_t trPerpWall1;
					bool bSoftBump1;
					bool bDir1 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * fHalfWidth * 2.0f, vForward, ePlacedBy, pTraceFilterPortalShot, trPerpWall1, bSoftBump1 );

					trace_t trPerpWall2;
					bool bSoftBump2;
					bool bDir2 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * -fHalfWidth * 2.0f, vForward, ePlacedBy, pTraceFilterPortalShot, trPerpWall2, bSoftBump2 );

					// No fit if there's blocking walls on both sides it can't fit
					if ( bDir1 && bDir2 )
					{
						if ( bSoftBump1 )
							bDir1 = false;
						else if ( bSoftBump2 )
							bDir1 = true;
						else
							return false;
					}

					// If there's no assumption to make, just pick a direction.
					if ( !bDir1 && !bDir2 )
					{
						bDir1 = true;
					}

					// Bump the portal
					if ( bDir1 )
					{
						vOrigin += sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * -fHalfWidth;
					}
					else
					{
						vOrigin += sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * fHalfWidth;
					}

					// Prepare data for recursion
					iIntersectionCount = 0;
					sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ 2 ] ].bCornerIntersection = false;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
				// Count similar intersections
				else if ( fDot[ iDot ] > 0.99f )
				{
					++iSimilarWalls;
				}
			}

			// If no intersections are similar
			if ( iSimilarWalls == 0 )
			{
				// Total the angles between the intersections
				float fAngleTotal = 0.0f;
				for ( int iDot = 0; iDot < 3; ++iDot )
				{
					fAngleTotal += acosf( fDot[ iDot ] );
				}

				// If it's in a triangle, it can't be fit
				if ( M_PI_F - 0.01f < fAngleTotal && fAngleTotal < M_PI_F + 0.01f )
				{
					// If any of the bumps are soft, give it another try
					if ( sFitData[ piIntersectionIndex[ 0 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 1 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 2 ] ].bSoftBump )
					{
						// Prepare data for recursion
						iIntersectionCount = 0;
						sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
						sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
						sFitData[ piIntersectionIndex[ 2 ] ].bCornerIntersection = false;

						return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
					}
					else
					{
						return false;
					}
				}
			}

			// If the intersections are all similar there's an easy way
			if ( iSimilarWalls == 3 )
			{
				// Get the closest intersection to the portal's center
				int iClosestIntersection = 0;
				float fClosestDistance = vOrigin.DistTo( sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint );

				float fDistance = vOrigin.DistTo( sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint );
				if ( fClosestDistance > fDistance )
				{
					iClosestIntersection = 1;
					fClosestDistance = fDistance;
				}

				fDistance = vOrigin.DistTo( sFitData[ piIntersectionIndex[ 2 ] ].ptIntersectionPoint );
				if ( fClosestDistance > fDistance )
				{
					iClosestIntersection = 2;
					fClosestDistance = fDistance;
				}

				// Find the largest amount that the portal needs to bump for the corner to pass the intersection
				float pfBumpDistance[ 3 ];

				for ( int iIntersection = 0; iIntersection < 3; ++iIntersection )
				{
					pfBumpDistance[ iIntersection ] = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIntersection ] ], 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vIntersectionDirection );
					pfBumpDistance[ iIntersection ] += PORTAL_BUMP_FORGIVENESS;
				}

				int iLargestBump = ( ( pfBumpDistance[ 0 ] > pfBumpDistance[ 1 ] ) ? ( 0 ) : ( 1 ) );

				iLargestBump = ( ( pfBumpDistance[ iLargestBump ] > pfBumpDistance[ 2 ] ) ? ( iLargestBump ) : ( 2 ) );

				// Bump the portal
				vOrigin += sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vBumpDirection * pfBumpDistance[ iLargestBump ];

				// Prepare data for recursion
				int iStillIntersecting = 0;

				for ( int iIntersection = 0; iIntersection < 3; ++iIntersection )
				{
					// Invalidate corners that were closer to the intersection line
					if ( pfBumpDistance[ iIntersection ] != pfBumpDistance[ iLargestBump ] )
					{
						sFitData[ piIntersectionIndex[ iIntersection ] ].bCornerIntersection = false;
						--iIntersectionCount;
					}
					else
					{
						sFitData[ piIntersectionIndex[ iIntersection ] ] = sFitData[ piIntersectionIndex[ iClosestIntersection ] ];
						piIntersectionIndex[ iStillIntersecting ] = piIntersectionIndex[ iIntersection ];
						++iStillIntersecting;
					}
				}

				return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
			}

			// Get info for which corners are diagonal from each other
			float fLongestDist = 0.0f;
			int iLongestDist = 0;

			for ( int iIntersection = 0; iIntersection < 3; ++iIntersection )
			{
				float fDist = pptCorner[ piIntersectionIndex[ iIntersection ] ].DistTo( pptCorner[ piIntersectionIndex[ ( iIntersection + 1 ) % 3 ] ] );

				if ( fLongestDist < fDist )
				{
					fLongestDist = fDist;
					iLongestDist = iIntersection;
				}
			}

			int iIndex1, iIndex2, iIndex3;

			switch ( iLongestDist )
			{
			case 0:
				iIndex1 = 0;
				iIndex2 = 1;
				iIndex3 = 2;
				break;

			case 1:
				iIndex1 = 1;
				iIndex2 = 2;
				iIndex3 = 0;
				break;

			default:
				iIndex1 = 2;
				iIndex2 = 0;
				iIndex3 = 1;
				break;
			}

			// If corner is 90 degrees there my be an easy way
			float fCornerDot = sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection.Dot( sFitData[ piIntersectionIndex[ iIndex2 ] ].vIntersectionDirection );

			if ( fCornerDot < 0.0001f && fCornerDot > -0.0001f )
			{
				// Check if portal is aligned perfectly with intersection normals
				float fPortalDot = sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection.Dot( vRight );

				if ( fPortalDot < 0.0001f && fPortalDot > -0.0001f || fPortalDot > 0.9999f || fPortalDot < -0.9999f )
				{
					float fBump1 = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIndex1 ] ], 
						sFitData[ piIntersectionIndex[ iIndex1 ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iIndex1 ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection );

					fBump1 += PORTAL_BUMP_FORGIVENESS;

					float fBump2 = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIndex2 ] ], 
						sFitData[ piIntersectionIndex[ iIndex2 ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iIndex2 ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iIndex2 ] ].vIntersectionDirection );

					fBump2 += PORTAL_BUMP_FORGIVENESS;

					// Bump portal
					vOrigin += sFitData[ piIntersectionIndex[ iIndex1 ] ].vBumpDirection * fBump1;
					vOrigin += sFitData[ piIntersectionIndex[ iIndex2 ] ].vBumpDirection * fBump2;

					// Prepare recursion data
					iIntersectionCount = 0;
					sFitData[ piIntersectionIndex[ iIndex1 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ iIndex2 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ iIndex3 ] ].bCornerIntersection = false;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
			}

			vOrigin += FindBumpVectorInCorner( pptCorner[ piIntersectionIndex[ iIndex1 ] ], pptCorner[ piIntersectionIndex[ iIndex2 ] ], 
				sFitData[ piIntersectionIndex[ iIndex1 ] ].ptIntersectionPoint, sFitData[ piIntersectionIndex[ iIndex2 ] ].ptIntersectionPoint,
				sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection, sFitData[ piIntersectionIndex[ iIndex2 ] ].vIntersectionDirection,
				sFitData[ piIntersectionIndex[ iIndex1 ] ].vBumpDirection, sFitData[ piIntersectionIndex[ iIndex2 ] ].vBumpDirection );

			// Prepare data for recursion
			iIntersectionCount = 0;
			sFitData[ piIntersectionIndex[ iIndex3 ] ].bCornerIntersection = false;

			return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );				
		}
		break;

	default:
		{	
			if ( sFitData[ piIntersectionIndex[ 0 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 1 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 2 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 3 ] ].bSoftBump )
			{
				// Prepare data for recursion
				iIntersectionCount = 0;
				sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 2 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 3 ] ].bCornerIntersection = false;

				return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, pTraceFilterPortalShot, fHalfWidth, fHalfHeight, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
			}
			else
			{
				// All corners intersect with no soft bumps, so it can't be fit
				return false;
			}
		}
		break;
	}

	return true;
}

void FitPortalAroundOtherPortals( const CProp_Portal *pIgnorePortal, Vector &vOrigin, const Vector &vForward, const Vector &vRight, const Vector &vUp, float fHalfWidth, float fHalfHeight )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal != pIgnorePortal && pTempPortal->IsActive() )
			{
				Vector vOtherOrigin = pTempPortal->GetAbsOrigin();
				QAngle qOtherAngles = pTempPortal->GetAbsAngles();

				Vector vLinkedForward;
				AngleVectors( qOtherAngles, &vLinkedForward, NULL, NULL );

				// If they're not on the same face then don't worry about overlap
				if ( (vForward.Dot( vLinkedForward ) < 0.95f) || 
					(fabs( vOrigin.Dot( vForward ) - vOtherOrigin.Dot( vLinkedForward ) ) > 1.0f) )
					continue;

				Vector vDiff = vOrigin - vOtherOrigin;

				Vector vDiffProjRight = vDiff.Dot( vRight ) * vRight;
				Vector vDiffProjUp = vDiff.Dot( vUp ) * vUp;

				float fProjRightLength = VectorNormalize( vDiffProjRight );
				float fProjUpLength = VectorNormalize( vDiffProjUp );

				if ( fProjRightLength < 1.0f )
				{
					vDiffProjRight = vRight;
				}

				if ( fProjUpLength < (fHalfHeight * 2.0f) && fProjRightLength < (fHalfWidth * 2.0f) )
				{
					vOrigin += vDiffProjRight * ( (fHalfWidth * 2.0f) - fProjRightLength + 1.0f );
				}
			}
		}
	}
}

bool IsPortalIntersectingNoPortalVolume( const Vector &vOrigin, const QAngle &qAngles, const Vector &vForward, float fHalfWidth, float fHalfHeight )
{
	// Walk the no portal volume list, check each with box-box intersection
	for ( CFuncNoPortalVolume *pNoPortalEnt = GetNoPortalVolumeList(); pNoPortalEnt != NULL; pNoPortalEnt = pNoPortalEnt->m_pNext )
	{
		// Skip inactive no portal zones
		if ( !pNoPortalEnt->IsActive() )
		{
			continue;
		}

		Vector vMin;
		Vector vMax;
		pNoPortalEnt->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

		Vector vBoxCenter = ( vMin + vMax ) * 0.5f;
		Vector vBoxExtents = ( vMax - vMin ) * 0.5f;

		// Take bump forgiveness into account on non major axies
		vBoxExtents += Vector( ( ( vForward.x > 0.5f || vForward.x < -0.5f ) ? ( 0.0f ) : ( -PORTAL_BUMP_FORGIVENESS ) ),
							   ( ( vForward.y > 0.5f || vForward.y < -0.5f ) ? ( 0.0f ) : ( -PORTAL_BUMP_FORGIVENESS ) ),
							   ( ( vForward.z > 0.5f || vForward.z < -0.5f ) ? ( 0.0f ) : ( -PORTAL_BUMP_FORGIVENESS ) ) );

		if ( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, vOrigin, qAngles, fHalfWidth, fHalfHeight ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				NDebugOverlay::Box( Vector( 0.0f, 0.0f, 0.0f ), vMin, vMax, 0, 255, 0, 128, 0.5f );
				UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );

				DevMsg( "Portal placed in no portal volume.\n" );
			}

			return true; 
		}
	}

	// Passed the list, so we didn't hit any func_noportal_volumes
	return false;
}

PortalPlacementResult_t IsPortalOverlappingOtherPortals( const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const QAngle &qAngles, float fHalfWidth, float fHalfHeight, 
														 bool bFizzleAll /*= false*/, bool bFizzlePartnerPortals /*= false*/ )
{
	bool bOverlappedLinkedPortal = false;
	bool bOverlappedPartnerPortal = false;

	Vector vForward;
	AngleVectors( qAngles, &vForward, NULL, NULL );

	Vector vPortalOBBMin = Vector( 0.0f, -fHalfWidth, -fHalfHeight );
	Vector vPortalOBBMax = Vector( 1.0f, fHalfWidth, fHalfHeight );

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal != pIgnorePortal && pTempPortal->IsActive() )
			{
				Vector vOtherOrigin = pTempPortal->GetAbsOrigin();
				QAngle qOtherAngles = pTempPortal->GetAbsAngles();
				Vector vOtherOBBMins = pTempPortal->GetLocalMins();
				Vector vOtherOBBMaxs = pTempPortal->GetLocalMaxs();

				Vector vLinkedForward;
				AngleVectors( qOtherAngles, &vLinkedForward, NULL, NULL );

				// If they're not on the same face then don't worry about overlap
				if ( vForward.Dot( vLinkedForward ) < 0.95f )
					continue;

				if ( IsOBBIntersectingOBB( vOrigin, qAngles, vPortalOBBMin, vPortalOBBMax, 
										   vOtherOrigin, qOtherAngles, vOtherOBBMins, vOtherOBBMaxs, 0.0f ) )
				{
					if ( sv_portal_placement_debug.GetBool() )
					{
						UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
						UTIL_Portal_NDebugOverlay( pTempPortal, 255, 0, 0, 128, false, 0.5f );

						DevMsg( "Portal overlapped another portal.\n" );
					}

					bool bLinkedPortal = !GameRules()->IsMultiplayer() || (pTempPortal->GetFiredByPlayer() == pIgnorePortal->GetFiredByPlayer());

					if ( bFizzleAll || ( !bLinkedPortal && bFizzlePartnerPortals ) )
					{
						pTempPortal->DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
						pTempPortal->Fizzle();

						if ( bLinkedPortal )
						{
							bOverlappedLinkedPortal = true;
						}
						else
						{
							bOverlappedPartnerPortal = true;
						}
					}
					else
					{
						if ( bLinkedPortal )
						{
							return PORTAL_PLACEMENT_OVERLAP_LINKED;
						}
						else
						{
							bOverlappedPartnerPortal = true;
						}
					}
				}
			}
		}
	}

	if ( bOverlappedLinkedPortal )
		return PORTAL_PLACEMENT_OVERLAP_LINKED;

	if ( bOverlappedPartnerPortal )
		return PORTAL_PLACEMENT_OVERLAP_PARTNER_PORTAL;

	return PORTAL_PLACEMENT_SUCCESS;
}

bool IsPortalOnValidSurface( const Vector &vOrigin, const Vector &vForward, const Vector &vRight, const Vector &vUp, float fHalfWidth, float fHalfHeight, ITraceFilter *traceFilterPortalShot )
{
	trace_t tr;

	// Check if corners are on a no portal material
	for ( int iCorner = 0; iCorner < 5; ++iCorner )
	{
		Vector ptCorner = vOrigin;

		if ( iCorner < 4 )
		{
			if ( iCorner / 2 == 0 )
				ptCorner += vUp * ( fHalfHeight - PORTAL_BUMP_FORGIVENESS * 1.1f ); //top
			else
				ptCorner += vUp * -( fHalfHeight - PORTAL_BUMP_FORGIVENESS * 1.1f ); //bottom

			if ( iCorner % 2 == 0 )
				ptCorner += vRight * -( fHalfWidth - PORTAL_BUMP_FORGIVENESS * 1.1f ); //left
			else
				ptCorner += vRight * ( fHalfWidth - PORTAL_BUMP_FORGIVENESS * 1.1f ); //right
		}

		Ray_t ray;
		ray.Init( ptCorner + vForward, ptCorner - vForward );
		enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, traceFilterPortalShot, &tr );

		if ( tr.startsolid )
		{
			// Portal center/corner in solid
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal center or corner placed inside solid.\n" );
			}

			return false;
		}

		if ( tr.fraction == 1.0f )
		{
			// Check if there's a portal bumper to act as a surface
			TraceBumpingEntities( ptCorner + vForward, ptCorner - vForward, tr );

			if ( tr.fraction == 1.0f )
			{
				// No surface behind the portal
				if ( sv_portal_placement_debug.GetBool() )
				{
					DevMsg( "Portal corner has no surface behind it.\n" );
				}

				return false;
			}
		}

		if ( tr.m_pEnt && FClassnameIs( tr.m_pEnt, "func_door" ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal placed on func_door.\n" );
			}

			return false;
		}

		if ( IsPassThroughMaterial( tr.surface ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal placed on a pass through material.\n" );
			}

			return false;
		}

		if ( IsNoPortalMaterial( tr ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal placed on a no portal material.\n" );
			}

			return false;
		}
	}

	return true;
}

PortalPlacementResult_t VerifyPortalPlacement( const CProp_Portal *pIgnorePortal, Vector &vOrigin, QAngle &qAngles, float fHalfWidth, float fHalfHeight, PortalPlacedBy_t ePlacedBy )
{
	Vector vOriginalOrigin = vOrigin;

	Vector vForward, vRight, vUp;
	AngleVectors( qAngles, &vForward, &vRight, &vUp );

	VectorNormalize( vForward );
	VectorNormalize( vRight );
	VectorNormalize( vUp );

	trace_t tr;
#if defined( GAME_DLL )
	CTraceFilterSimpleClassnameList baseFilter( pIgnorePortal, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	baseFilter.AddClassnameToIgnore( "prop_portal" );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );
#else
	CTraceFilterSimpleClassnameList traceFilterPortalShot( pIgnorePortal, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &traceFilterPortalShot );
	traceFilterPortalShot.AddClassnameToIgnore( "prop_portal" );
#endif

	// Check if center is on a surface
	Ray_t ray;
	ray.Init( vOrigin + vForward, vOrigin - vForward );
	enginetrace->TraceRay( ray, MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

	if ( tr.fraction == 1.0f )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
			DevMsg( "Portal center has no surface behind it.\n" );
		}

		return PORTAL_PLACEMENT_INVALID_SURFACE;
	}

	// Check if the surface is moving
	/*Vector vVelocityCheck;
	AngularImpulse vAngularImpulseCheck;

	IPhysicsObject *pPhysicsObject = tr.m_pEnt->VPhysicsGetObject();

	if ( pPhysicsObject )
	{
		pPhysicsObject->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );
	}
	else
	{
#if defined( GAME_DLL )
		tr.m_pEnt->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );
#else
		vVelocityCheck = tr.m_pEnt->GetAbsVelocity();
		vAngularImpulseCheck = vec3_origin; //TODO: Find client equivalent of server code above for angular impulse
#endif
	}*/

	if ( !sv_allow_mobile_portals.GetBool() && UTIL_IsEntityMovingOrRotating( tr.m_pEnt )/*(vVelocityCheck != vec3_origin || vAngularImpulseCheck != vec3_origin)*/ )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			DevMsg( "Portal was on moving surface.\n" );
		}

		return PORTAL_PLACEMENT_INVALID_SURFACE;
	}

	// Check for invalid materials
	if ( IsPassThroughMaterial( tr.surface ) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
			DevMsg( "Portal placed on a pass through material.\n" );
		}

		return PORTAL_PLACEMENT_PASSTHROUGH_SURFACE;
	}

	if ( IsNoPortalMaterial( tr ) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
			DevMsg( "Portal placed on a no portal material.\n" );
		}

		return PORTAL_PLACEMENT_INVALID_SURFACE;
	}

	// Get pointer to liked portal if it might be in the way
	g_bBumpedByLinkedPortal = false;

	if ( ePlacedBy == PORTAL_PLACED_BY_PLAYER && !sv_portal_placement_never_bump.GetBool() )
	{
		// Bump away from linked portal so it can be fit next to it
		FitPortalAroundOtherPortals( pIgnorePortal, vOrigin, vForward, vRight, vUp, fHalfWidth, fHalfHeight );
	}

	float fBumpDistance = 0.0f;

	if ( !sv_portal_placement_never_bump.GetBool() )
	{
		// Fit onto surface and auto bump
		g_FuncBumpingEntityList.RemoveAll();

		Vector vTopEdge = vUp * ( fHalfHeight - PORTAL_BUMP_FORGIVENESS );
		Vector vBottomEdge = -vTopEdge;
		Vector vRightEdge = vRight * ( fHalfWidth - PORTAL_BUMP_FORGIVENESS );
		Vector vLeftEdge = -vRightEdge;

		if ( !FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, ePlacedBy, &traceFilterPortalShot, fHalfWidth, fHalfHeight ) )
		{
			if ( g_bBumpedByLinkedPortal )
			{
				return PORTAL_PLACEMENT_OVERLAP_LINKED;
			}

			if ( sv_portal_placement_debug.GetBool() )
			{
				UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
				DevMsg( "Portal was unable to fit on surface.\n" );
			}

			return PORTAL_PLACEMENT_CANT_FIT;
		}

		// Check if it's moved too far from it's original location
		fBumpDistance = vOrigin.DistToSqr( vOriginalOrigin );

		if ( fBumpDistance > MAXIMUM_BUMP_DISTANCE )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
				DevMsg( "Portal adjusted too far from it's original location.\n" );
			}

			return PORTAL_PLACEMENT_CANT_FIT;
		}

		//if we're less than a unit from floor, we're going to bump to match it exactly and help game movement code run smoothly
		if( vUp.z > 0.7f )
		{
			Vector vSmallForward = vForward * 0.05f;
			trace_t FloorTrace;
			UTIL_TraceLine( vOrigin + vSmallForward, vOrigin + vSmallForward - (vUp * (fHalfHeight + 1.5f)), MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &FloorTrace );
			if( FloorTrace.fraction < 1.0f )
			{
				//we hit floor in that 1 extra unit, now doublecheck to make sure we didn't hit something else
				trace_t FloorTrace_Verify;
				UTIL_TraceLine( vOrigin + vSmallForward, vOrigin + vSmallForward - (vUp * (fHalfHeight - 0.1f)), MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &FloorTrace_Verify );
				if( FloorTrace_Verify.fraction == 1.0f )
				{
					//if we're in here, we're definitely in a floor matching configuration, bump down to match the floor better
					vOrigin = FloorTrace.endpos + (vUp * fHalfHeight) - vSmallForward;// - vUp * PORTAL_WALL_MIN_THICKNESS;
				}
			}
		}
	}

	// Fail if it's in a no portal volume
	if ( IsPortalIntersectingNoPortalVolume( vOrigin, qAngles, vForward, fHalfWidth, fHalfHeight ) )
		return PORTAL_PLACEMENT_INVALID_VOLUME;

	// Fail if it's overlapping the linked portal
	PortalPlacementResult_t ePortalOverlapResult = IsPortalOverlappingOtherPortals( pIgnorePortal, vOrigin, qAngles, fHalfWidth, fHalfHeight );
	if ( ePortalOverlapResult != PORTAL_PLACEMENT_SUCCESS )
		return ePortalOverlapResult;

	// Fail if it's on a flagged surface material
	if ( !IsPortalOnValidSurface( vOrigin, vForward, vRight, vUp, fHalfWidth, fHalfHeight, &traceFilterPortalShot ) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, fHalfWidth, fHalfHeight, 0, 0, 255, 128, false, 0.5f );
		}
		return PORTAL_PLACEMENT_INVALID_SURFACE;
	}

	// Is a floor being moved to another floor
	if ( pIgnorePortal->IsFloorPortal() && vForward.z > 0.8f )
	{
		// Is it being moved close by
		if ( pIgnorePortal->GetAbsOrigin().DistToSqr( vOrigin ) < ( fHalfWidth * fHalfWidth + fHalfHeight * fHalfHeight ) )
		{
			// Are there any players intersecting it?
			for( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer *pIntersectingPlayer = UTIL_PlayerByIndex( i );
				if ( pIntersectingPlayer )
				{
					if ( pIgnorePortal->m_PortalSimulator.IsReadyToSimulate() )
					{
						if ( UTIL_Portal_EntityIsInPortalHole( pIgnorePortal, pIntersectingPlayer ) )
						{
							// Fail! This created the portal vert hop exploit
							return PORTAL_PLACEMENT_OVERLAP_LINKED;
						}
					}
				}
			}
		}
	}

	return PORTAL_PLACEMENT_BUMPED;
}

PortalPlacementResult_t VerifyPortalPlacementAndFizzleBlockingPortals( const CProp_Portal *pIgnorePortal, Vector &vOrigin, QAngle &qAngles, float fHalfWidth, float fHalfHeight, PortalPlacedBy_t ePlacedBy )
{
	PortalPlacementResult_t placementResult = VerifyPortalPlacement( pIgnorePortal, vOrigin, qAngles, fHalfWidth, fHalfHeight, ePlacedBy );
	if ( ePlacedBy == PORTAL_PLACED_BY_FIXED && placementResult == PORTAL_PLACEMENT_OVERLAP_LINKED )
	{
		// overlapping another portal. Fizzle them and try again.
		IsPortalOverlappingOtherPortals( pIgnorePortal, vOrigin, qAngles, fHalfWidth, fHalfHeight, true, false );
		placementResult = VerifyPortalPlacement( pIgnorePortal, vOrigin, qAngles, fHalfWidth, fHalfHeight, ePlacedBy );
	}
	else if ( ePlacedBy == PORTAL_PLACED_BY_PLAYER && placementResult == PORTAL_PLACEMENT_OVERLAP_PARTNER_PORTAL )
	{
		CPortalMPGameRules *pRules = PortalMPGameRules();
		if( pRules && pRules->IsVS() )
		{
			return placementResult;
		}

		// overlapping partner's portal. Fizzle them and try again.
		IsPortalOverlappingOtherPortals( pIgnorePortal, vOrigin, qAngles, fHalfWidth, fHalfHeight, false, true );
		placementResult = VerifyPortalPlacement( pIgnorePortal, vOrigin, qAngles, fHalfWidth, fHalfHeight, ePlacedBy );
	}

	return placementResult;
}

//-----------------------------------------------------------------------------
// Purpose: Breaks the granular placement result down into a binary "succeed/fail" state
//-----------------------------------------------------------------------------
bool PortalPlacementSucceeded( PortalPlacementResult_t eResult )
{
	switch ( eResult )
	{
	case PORTAL_PLACEMENT_SUCCESS:
	case PORTAL_PLACEMENT_BUMPED:
	case PORTAL_PLACEMENT_USED_HELPER:
		return true;

	default:
		return false;
	}
}
