//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <algorithm>
#include "portal_util_shared.h"
#include "portal_base2d_shared.h"
#include "portal_shareddefs.h"
#include "portal_collideable_enumerator.h"
#include "beam_shared.h"
#include "CollisionUtils.h"
#include "util_shared.h"
#include "portal_mp_gamerules.h"
#include "coordsize.h"

#ifndef CLIENT_DLL
	#include "Util.h"
	#include "NDebugOverlay.h"
	#include "env_debughistory.h"
	#include "world.h"
#else
	#include "c_portal_player.h"
	#include "c_prop_portal.h"
	#include "materialsystem/imaterialvar.h"
	#include "c_world.h"
#endif
#include "PortalSimulation.h"
#include "CegClientWrapper.h"

bool g_bAllowForcePortalTrace = false;
bool g_bForcePortalTrace = false;
bool g_bBulletPortalTrace = false;

// paint convars
ConVar sv_paint_detection_sphere_radius( "sv_paint_detection_sphere_radius", "16.f", FCVAR_REPLICATED | FCVAR_CHEAT, "The radius of the query sphere used to find the color of a light map at a contact point in world space." );


ConVar sv_portal_trace_vs_world ("sv_portal_trace_vs_world", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment world geometry" );
ConVar sv_portal_trace_vs_displacements ("sv_portal_trace_vs_displacements", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment displacement geometry" );
ConVar sv_portal_trace_vs_holywall ("sv_portal_trace_vs_holywall", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment carved wall" );
ConVar sv_portal_trace_vs_staticprops ("sv_portal_trace_vs_staticprops", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Use traces against portal environment static prop geometry" );
ConVar sv_use_find_closest_passable_space ("sv_use_find_closest_passable_space", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Enables heavy-handed player teleporting stuck fix code." );
ConVar sv_use_transformed_collideables("sv_use_transformed_collideables", "1", FCVAR_REPLICATED | FCVAR_CHEAT, "Disables traces against remote portal moving entities using transforms to bring them into local space." );

ConVar portal_trace_shrink_ray_each_query("portal_trace_shrink_ray_each_query", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar portal_beamtrace_optimization ("portal_beamtrace_optimization", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
// FIXME: Bring this back for DLC2
//ConVar reflect_paint_vertical_snap( "reflect_paint_vertical_snap", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

extern ConVar portal_clone_displacements;

const float COS_PI_OVER_SIX = 0.86602540378443864676372317075294f; // cos( 30 degrees ) in radians

//#define PORTAL_TRACE_LOGGING

#if defined PORTAL_TRACE_LOGGING
enum eTraceType
{
	BRUSHES,
	DISP,
	HOLYWALL_BRUSHES,
	HOLYWALL_TUBE,
	HOLYWALL_TRANSLATED_BRUSHES,
	ENTITIES,
	STATIC_PROPS,
	REMOTE_STATIC_PROPS,
	TRACE_TYPE_COUNT,
};
class CPortalTraceLogger 
{
public:
	CPortalTraceLogger::CPortalTraceLogger():m_nTotalTraces(0), m_nTotalKept(0)
	{
		for (int i=0; i<TRACE_TYPE_COUNT;++i)
		{
			m_nCallCounts[i] = 0;
			m_nKeptCounts[i] = 0;
		}
	}

	void LogTrace( eTraceType type ) { m_nCallCounts[type]++; m_nTotalTraces++; }
	void LogTypeKept( eTraceType type ) { m_nKeptCounts[type]++; m_nTotalKept++; }
	void Display()
	{
		char row = 20;
		for ( int i = 0; i < TRACE_TYPE_COUNT; ++i )
		{
			engine->Con_NPrintf( row++, "Calls(%d): %d [%f.2]", i, m_nCallCounts[i], 100.0f * ((float)m_nCallCounts[i]/(float)m_nTotalTraces) );
			engine->Con_NPrintf( row++, "Keeps(%d): %d [%f.2]", i, m_nKeptCounts[i], 100.0f * ((float)m_nKeptCounts[i]/(float)m_nTotalKept) );
		}
	}
	uint32	m_nCallCounts[TRACE_TYPE_COUNT];	
	uint32	m_nKeptCounts[TRACE_TYPE_COUNT];	
	uint64	m_nTotalTraces;
	uint64	m_nTotalKept;
};

static CPortalTraceLogger s_TraceLogger;

CON_COMMAND( dump_portal_trace_log, "Spew current trace data to the console" )
{
	for ( int i = 0; i < TRACE_TYPE_COUNT; ++i )
	{
		Msg( "Calls(%d): %d [%f.2]\n", i, s_TraceLogger.m_nCallCounts[i], 100.0f * ((float)s_TraceLogger.m_nCallCounts[i]/(float)s_TraceLogger.m_nTotalTraces) );
		Msg( "Keeps(%d): %d [%f.2]\n", i, s_TraceLogger.m_nKeptCounts[i], 100.0f * ((float)s_TraceLogger.m_nKeptCounts[i]/(float)s_TraceLogger.m_nTotalKept) );
	}
}

#endif // PORTAL_TRACE_LOGGING


class CTransformedCollideable : public ICollideable //wraps an existing collideable, but transforms everything that pertains to world space by another transform
{
public:
	VMatrix m_matTransform; //the transformation we apply to the wrapped collideable
	VMatrix m_matInvTransform; //cached inverse of m_matTransform

	ICollideable *m_pWrappedCollideable; //the collideable we're transforming without it knowing

	struct CTC_ReferenceVars_t
	{
		Vector m_vCollisionOrigin;
		QAngle m_qCollisionAngles;
		matrix3x4_t m_matCollisionToWorldTransform;
		matrix3x4_t m_matRootParentToWorldTransform;
	}; 

	mutable CTC_ReferenceVars_t m_ReferencedVars; //when returning a const reference, it needs to point to something, so here we go

	//abstract functions which require no transforms, just pass them along to the wrapped collideable
	virtual IHandleEntity	*GetEntityHandle() { return m_pWrappedCollideable->GetEntityHandle(); }
	virtual const Vector&	OBBMins() const { return m_pWrappedCollideable->OBBMins(); };
	virtual const Vector&	OBBMaxs() const { return m_pWrappedCollideable->OBBMaxs(); };
	virtual int				GetCollisionModelIndex() { return m_pWrappedCollideable->GetCollisionModelIndex(); };
	virtual const model_t*	GetCollisionModel() { return m_pWrappedCollideable->GetCollisionModel(); };
	virtual SolidType_t		GetSolid() const { return m_pWrappedCollideable->GetSolid(); };
	virtual int				GetSolidFlags() const { return m_pWrappedCollideable->GetSolidFlags(); };
	virtual IClientUnknown*	GetIClientUnknown() { return m_pWrappedCollideable->GetIClientUnknown(); };
	virtual int				GetCollisionGroup() const { return m_pWrappedCollideable->GetCollisionGroup(); };
	virtual uint			GetRequiredTriggerFlags() const { return m_pWrappedCollideable->GetRequiredTriggerFlags(); }
	virtual IPhysicsObject	*GetVPhysicsObject() const { return m_pWrappedCollideable->GetVPhysicsObject(); }

	//slightly trickier functions
	virtual void			WorldSpaceTriggerBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs ) const;
	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual bool			TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual const Vector&	GetCollisionOrigin() const;
	virtual const QAngle&	GetCollisionAngles() const;
	virtual const matrix3x4_t&	CollisionToWorldTransform() const;
	virtual void			WorldSpaceSurroundingBounds( Vector *pVecMins, Vector *pVecMaxs );
	virtual const matrix3x4_t	*GetRootParentToWorldTransform() const;
};

void CTransformedCollideable::WorldSpaceTriggerBounds( Vector *pVecWorldMins, Vector *pVecWorldMaxs ) const
{
	m_pWrappedCollideable->WorldSpaceTriggerBounds( pVecWorldMins, pVecWorldMaxs );
	
	if( pVecWorldMins )
		*pVecWorldMins = m_matTransform * (*pVecWorldMins);

	if( pVecWorldMaxs )
		*pVecWorldMaxs = m_matTransform * (*pVecWorldMaxs);
}

bool CTransformedCollideable::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	//TODO: Transform the ray by inverse matTransform and transform the trace results by matTransform? AABB Errors arise by transforming the ray.
    return m_pWrappedCollideable->TestCollision( ray, fContentsMask, tr );
}

bool CTransformedCollideable::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	//TODO: Transform the ray by inverse matTransform and transform the trace results by matTransform? AABB Errors arise by transforming the ray.
	return m_pWrappedCollideable->TestHitboxes( ray, fContentsMask, tr );
}

const Vector& CTransformedCollideable::GetCollisionOrigin() const
{
	m_ReferencedVars.m_vCollisionOrigin = m_matTransform * m_pWrappedCollideable->GetCollisionOrigin();
	return m_ReferencedVars.m_vCollisionOrigin;
}

const QAngle& CTransformedCollideable::GetCollisionAngles() const
{
	m_ReferencedVars.m_qCollisionAngles = TransformAnglesToWorldSpace( m_pWrappedCollideable->GetCollisionAngles(), m_matTransform.As3x4() );
	return m_ReferencedVars.m_qCollisionAngles;
}

const matrix3x4_t& CTransformedCollideable::CollisionToWorldTransform() const
{
	//1-2 order correct?
	ConcatTransforms( m_matTransform.As3x4(), m_pWrappedCollideable->CollisionToWorldTransform(), m_ReferencedVars.m_matCollisionToWorldTransform );
	return m_ReferencedVars.m_matCollisionToWorldTransform;
}

void CTransformedCollideable::WorldSpaceSurroundingBounds( Vector *pVecMins, Vector *pVecMaxs )
{
	if( (pVecMins == NULL) && (pVecMaxs == NULL) )
		return;

	Vector vMins, vMaxs;
	m_pWrappedCollideable->WorldSpaceSurroundingBounds( &vMins, &vMaxs );

	TransformAABB( m_matTransform.As3x4(), vMins, vMaxs, vMins, vMaxs );

	if( pVecMins )
		*pVecMins = vMins;
	if( pVecMaxs )
		*pVecMaxs = vMaxs;
}

const matrix3x4_t* CTransformedCollideable::GetRootParentToWorldTransform() const
{
	const matrix3x4_t *pWrappedVersion = m_pWrappedCollideable->GetRootParentToWorldTransform();
	if( pWrappedVersion == NULL )
		return NULL;

	ConcatTransforms( m_matTransform.As3x4(), *pWrappedVersion, m_ReferencedVars.m_matRootParentToWorldTransform );
	return &m_ReferencedVars.m_matRootParentToWorldTransform;
}

#if defined ( CLIENT_DLL )
static const Color s_defaultPortalColors[2] = { Color( 64, 160, 255, 255 ), Color( 255, 160, 32, 255 ) };

Color UTIL_Portal_Color( int iPortal, int iTeamNumber /*= 0*/ )
{
	switch ( iPortal )
	{
		case 0:
			// GRAVITY BEAM
			return Color( 242, 202, 167, 255 );

		case 1:
		case 2:
		{
			// PORTAL 1 or 2
			if ( GameRules()->IsMultiplayer() && !((CPortalMPGameRules *)g_pGameRules)->Is2GunsCoOp() )
			{
				Assert( TEAM_BLUE == TEAM_RED + 1 );

				if ( ( iTeamNumber == TEAM_RED ) || ( iTeamNumber == TEAM_BLUE ) )
				{
					const Vector &vColor = C_Prop_Portal::m_Materials.m_coopPlayerPortalColors[ 1 - ( iTeamNumber - TEAM_RED ) ][ iPortal - 1 ];
					return Color( vColor[0] * 255, vColor[1] * 255, vColor[2] * 255 );
				}
				else
				{
					return s_defaultPortalColors[ iPortal - 1 ];
				}
			}
			else
			{
				// Single player
				const Vector &vColor = C_Prop_Portal::m_Materials.m_singlePlayerPortalColors[ iPortal - 1 ];
				return Color( vColor[0] * 255, vColor[1] * 255, vColor[2] * 255 );
			}
		}
		break;
	}

	Assert( 0 );
	return Color( 255, 255, 255, 255 );
}

Color UTIL_Portal_Color_Particles( int iPortal, int iTeamNumber /*= 0*/ )
{
	if ( GameRules()->IsMultiplayer() && !((CPortalMPGameRules *)g_pGameRules)->Is2GunsCoOp() )
		return UTIL_Portal_Color( iPortal, iTeamNumber );
	else
		return (iPortal - 1)?( Color( 233, 78, 2, 255 ) ):( Color( 0, 60, 255, 255 ) );
}
#endif

void UTIL_Portal_Trace_Filter( CTraceFilterSimpleClassnameList *traceFilterPortalShot )
{
	traceFilterPortalShot->AddClassnameToIgnore( "prop_physics" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_weighted_cube" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_monster_box" );
	traceFilterPortalShot->AddClassnameToIgnore( "func_physbox" );
	traceFilterPortalShot->AddClassnameToIgnore( "npc_portal_turret_floor" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_energy_ball" );
	traceFilterPortalShot->AddClassnameToIgnore( "npc_security_camera" );
	traceFilterPortalShot->AddClassnameToIgnore( "simple_physics_prop" );
	traceFilterPortalShot->AddClassnameToIgnore( "simple_physics_brush" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_ragdoll" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_glados_core" );
	traceFilterPortalShot->AddClassnameToIgnore( "player" );
	traceFilterPortalShot->AddClassnameToIgnore( "Player" );
	traceFilterPortalShot->AddClassnameToIgnore( "projected_wall_entity" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_paint_bomb" );
	traceFilterPortalShot->AddClassnameToIgnore( "prop_exploding_futbol" );
	traceFilterPortalShot->AddClassnameToIgnore( "npc_personality_core" );
}


CPortal_Base2D* UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fMustBeCloserThan, CPortal_Base2D **pSearchArray, int iSearchArrayCount )
{
	CPortal_Base2D *pIntersectedPortal = NULL;

	for( int i = 0; i != iSearchArrayCount; ++i )
	{
		CPortal_Base2D *pTempPortal = pSearchArray[i];
		if( pTempPortal->IsActivedAndLinked() )
		{
			float fIntersection = UTIL_IntersectRayWithPortal( ray, pTempPortal );
			if( fIntersection >= 0.0f && fIntersection < fMustBeCloserThan )
			{
				//within range, now check directionality
				if( pTempPortal->m_plane_Origin.normal.Dot( ray.m_Delta ) < 0.0f )
				{
					//qualifies for consideration, now it just has to compete for closest
					pIntersectedPortal = pTempPortal;
					fMustBeCloserThan = fIntersection;
				}
			}
		}
	}

	return pIntersectedPortal;
}


CPortal_Base2D* UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fMustBeCloserThan )
{
	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
		return UTIL_Portal_FirstAlongRay( ray, fMustBeCloserThan, pPortals, iPortalCount );
	}

	return NULL;
}

bool UTIL_Portal_TraceRay_Bullets( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall )
{
	if( !pPortal || !pPortal->IsActivedAndLinked() )
	{
		//not in a portal environment, use regular traces
		enginetrace->TraceRay( ray, fMask, pTraceFilter, pTrace );
		return false;
	}

	trace_t trReal;

	enginetrace->TraceRay( ray, fMask, pTraceFilter, &trReal );

	Vector vRayNormal = ray.m_Delta;
	VectorNormalize( vRayNormal );

	// If the ray isn't going into the front of the portal, just use the real trace
	if ( pPortal->m_vForward.Dot( vRayNormal ) > 0.0f )
	{
		*pTrace = trReal;
		return false;
	}

	// If the real trace collides before the portal plane, just use the real trace
	float fPortalFraction = UTIL_IntersectRayWithPortal( ray, pPortal );

	if ( fPortalFraction == -1.0f || trReal.fraction + 0.0001f < fPortalFraction )
	{
		// Didn't intersect or the real trace intersected closer
		*pTrace = trReal;
		return false;
	}

	Ray_t rayPostPortal;
	rayPostPortal = ray;
	rayPostPortal.m_Start = ray.m_Start + ray.m_Delta * fPortalFraction;
	rayPostPortal.m_Delta = ray.m_Delta * ( 1.0f - fPortalFraction );

	VMatrix matThisToLinked = pPortal->MatrixThisToLinked();

	Ray_t rayTransformed;
	UTIL_Portal_RayTransform( matThisToLinked, rayPostPortal, rayTransformed );

	// After a bullet traces through a portal it can hit the player that fired it
	CTraceFilterSimple *pSimpleFilter = dynamic_cast<CTraceFilterSimple*>(pTraceFilter);
	const IHandleEntity *pPassEntity = NULL;
	if ( pSimpleFilter )
	{
		pPassEntity = pSimpleFilter->GetPassEntity();
		pSimpleFilter->SetPassEntity( 0 );
	}

	trace_t trPostPortal;
	enginetrace->TraceRay( rayTransformed, fMask, pTraceFilter, &trPostPortal );

	if ( pSimpleFilter )
	{
		pSimpleFilter->SetPassEntity( pPassEntity );
	}

	//trPostPortal.startpos = ray.m_Start;
	UTIL_Portal_PointTransform( matThisToLinked, ray.m_Start, trPostPortal.startpos );
	trPostPortal.fraction = trPostPortal.fraction * ( 1.0f - fPortalFraction ) + fPortalFraction;

	*pTrace = trPostPortal;

	return true;
}

CPortal_Base2D* UTIL_Portal_TraceRay_Beam( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, float *pfFraction )
{
	// Do a regular trace
	trace_t tr;
	UTIL_TraceLine( ray.m_Start, ray.m_Start + ray.m_Delta, fMask, pTraceFilter, &tr );
	float fMustBeCloserThan = tr.fraction + 0.0001f;

	CPortal_Base2D *pIntersectedPortal = UTIL_Portal_FirstAlongRay( ray, fMustBeCloserThan );

	*pfFraction = fMustBeCloserThan; //will be real trace distance if it didn't hit a portal
	return pIntersectedPortal;
}


void UTIL_Portal_TraceRay_With( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall )
{
	//check to see if the player is theoretically in a portal environment
	if( !pPortal || !pPortal->m_PortalSimulator.IsReadyToSimulate() )
	{
		//not in a portal environment, use regular traces
		enginetrace->TraceRay( ray, fMask, pTraceFilter, pTrace );
	}
	else
	{		

		trace_t RealTrace;
		enginetrace->TraceRay( ray, fMask, pTraceFilter, &RealTrace );

		trace_t PortalTrace;
		UTIL_Portal_TraceRay( pPortal, ray, fMask, pTraceFilter, &PortalTrace, bTraceHolyWall );

		if( !g_bForcePortalTrace && !RealTrace.startsolid && PortalTrace.fraction < RealTrace.fraction )
		{
			*pTrace = RealTrace;
			return;
		}

		if ( g_bAllowForcePortalTrace )
		{
			g_bForcePortalTrace = true;
		}

		*pTrace = PortalTrace;

		// If this ray has a delta, make sure its towards the portal before we try to trace across portals
		Vector vDirection = ray.m_Delta;
		VectorNormalize( vDirection );
		
		float flDot = -1.0f;
		if ( ray.m_IsSwept )
		{
			flDot = vDirection.Dot( pPortal->m_vForward );
		} 

		// TODO: Translate extents of rays properly, tracing extruded box rays across portals causes collision bugs
		//		 Until this is fixed, we'll only test true rays across portals
		if ( flDot < 0.0f && /*PortalTrace.fraction == 1.0f &&*/ ray.m_IsRay)
		{
			// Check if we're hitting stuff on the other side of the portal
			trace_t PortalLinkedTrace;
			UTIL_PortalLinked_TraceRay( pPortal, ray, fMask, pTraceFilter, &PortalLinkedTrace, bTraceHolyWall );

			if ( PortalLinkedTrace.fraction < pTrace->fraction )
			{
				// Only collide with the cross-portal objects if this trace crossed a portal
				if ( UTIL_DidTraceTouchPortals( ray, PortalLinkedTrace ) )
				{
					*pTrace = PortalLinkedTrace;
				}
			}
		}

		if( pTrace->fraction < 1.0f )
		{
			pTrace->contents = RealTrace.contents;
			pTrace->surface = RealTrace.surface;
		}

	}
}


//-----------------------------------------------------------------------------
// Purpose: Tests if a ray touches the surface of any portals
// Input  : ray - the ray to be tested against portal surfaces
//			trace - a filled-in trace corresponding to the parameter ray 
// Output : bool - false if the 'ray' parameter failed to hit any portal surface
//		    pOutLocal - the portal touched (if any)
//			pOutRemote - the portal linked to the portal touched
//-----------------------------------------------------------------------------
bool UTIL_DidTraceTouchPortals( const Ray_t& ray, const trace_t& trace, CPortal_Base2D** pOutLocal, CPortal_Base2D** pOutRemote )
{
	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
	{
		if( pOutLocal )
			*pOutLocal = NULL;

		if( pOutRemote )
			*pOutRemote = NULL;

		return false;
	}

	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	CPortal_Base2D *pIntersectedPortal = NULL;

	if( ray.m_IsSwept )
	{
		float fMustBeCloserThan = trace.fraction + 0.0001f;

		pIntersectedPortal = UTIL_Portal_FirstAlongRay( ray, fMustBeCloserThan );
	}
	
	if( (pIntersectedPortal == NULL) && !ray.m_IsRay )
	{
		//haven't hit anything yet, try again with box tests

		Vector ptRayEndPoint = trace.endpos - ray.m_StartOffset; // The trace added the start offset to the end position, so remove it for the box test
		CPortal_Base2D **pBoxIntersectsPortals = (CPortal_Base2D **)stackalloc( sizeof(CPortal_Base2D *) * iPortalCount );
		int iBoxIntersectsPortalsCount = 0;

		for( int i = 0; i != iPortalCount; ++i )
		{
			CPortal_Base2D *pTempPortal = pPortals[i];
			if( (pTempPortal->IsActive()) && 
				(pTempPortal->m_hLinkedPortal.Get() != NULL) )
			{
				if( UTIL_IsBoxIntersectingPortal( ptRayEndPoint, ray.m_Extents, pTempPortal, 0.00f ) )
				{
					pBoxIntersectsPortals[iBoxIntersectsPortalsCount] = pTempPortal;
					++iBoxIntersectsPortalsCount;
				}
			}
		}

		if( iBoxIntersectsPortalsCount > 0 )
		{
			pIntersectedPortal = pBoxIntersectsPortals[0];
			
			if( iBoxIntersectsPortalsCount > 1 )
			{
				//hit more than one, use the closest
				float fDistToBeat = (ptRayEndPoint - pIntersectedPortal->m_ptOrigin).LengthSqr();

				for( int i = 1; i != iBoxIntersectsPortalsCount; ++i )
				{
					float fDist = (ptRayEndPoint - pBoxIntersectsPortals[i]->m_ptOrigin).LengthSqr();
					if( fDist < fDistToBeat )
					{
						pIntersectedPortal = pBoxIntersectsPortals[i];
						fDistToBeat = fDist;
					}
				}
			}
		}
	}

	if( pIntersectedPortal == NULL )
	{
		if( pOutLocal )
			*pOutLocal = NULL;

		if( pOutRemote )
			*pOutRemote = NULL;

		return false;
	}
	else
	{
		// Record the touched portals and return
		if( pOutLocal )
			*pOutLocal = pIntersectedPortal;

		if( pOutRemote )
			*pOutRemote = pIntersectedPortal->m_hLinkedPortal.Get();

		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Redirects the trace to either a trace that uses portal environments, or if a 
//			global boolean is set, trace with a special bullets trace.
//			NOTE: UTIL_Portal_TraceRay_With will use the default world trace if it gets a NULL portal pointer
// Input  : &ray - the ray to use to trace
//			fMask - collision mask
//			*pTraceFilter - customizable filter on the trace
//			*pTrace - trace struct to fill with output info
//-----------------------------------------------------------------------------
CPortal_Base2D* UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall )
{
	float fMustBeCloserThan = 2.0f;
	CPortal_Base2D *pIntersectedPortal = UTIL_Portal_FirstAlongRay( ray, fMustBeCloserThan );

	if ( g_bBulletPortalTrace )
	{
		if ( UTIL_Portal_TraceRay_Bullets( pIntersectedPortal, ray, fMask, pTraceFilter, pTrace, bTraceHolyWall ) )
			return pIntersectedPortal;

		// Bullet didn't actually go through portal
		return NULL;

	}
	else
	{
		UTIL_Portal_TraceRay_With( pIntersectedPortal, ray, fMask, pTraceFilter, pTrace, bTraceHolyWall );
		return pIntersectedPortal;
	}
}

CPortal_Base2D* UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall )
{
	CTraceFilterSimple traceFilter( ignore, collisionGroup );
	return UTIL_Portal_TraceRay( ray, fMask, &traceFilter, pTrace, bTraceHolyWall );
}


extern ConVar sv_portal_new_player_trace;
//-----------------------------------------------------------------------------
// Purpose: This version of traceray only traces against the portal environment of the specified portal.
// Input  : *pPortal - the portal whose physics we will trace against
//			&ray - the ray to trace with
//			fMask - collision mask
//			*pTraceFilter - customizable filter to determine what it hits
//			*pTrace - the trace struct to fill in with results
//			bTraceHolyWall - if this trace is to test against the 'holy wall' geometry
//-----------------------------------------------------------------------------
//extern bool g_bSpewTraceStuck;
void UTIL_Portal_TraceRay_PreTraceChanges( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall );
void UTIL_Portal_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall )
{
	if( !sv_portal_new_player_trace.GetBool() )
	{
		//it's not funny anymore, go back
		return UTIL_Portal_TraceRay_PreTraceChanges( pPortal, ray, fMask, pTraceFilter, pTrace, bTraceHolyWall );
	}

#if defined ( PORTAL_TRACE_LOGGING )
	int keptType = -1;
#endif

	Assert( pPortal->m_PortalSimulator.IsReadyToSimulate() ); //a trace shouldn't make it down this far if the portal is incapable of changing the results of the trace

	CTraceFilterHitAll traceFilterHitAll;
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilterHitAll;
	}

	UTIL_ClearTrace( *pTrace );
	pTrace->startpos = ray.m_Start + ray.m_StartOffset;
	pTrace->endpos = pTrace->startpos + ray.m_Delta;

	// We shrink the ray when we hit something so subsequent traces can early-out on hits that would be further away
	Ray_t queryRay = ray;

	trace_t TempTrace;
	int counter;

	const CPortalSimulator &portalSimulator = pPortal->m_PortalSimulator;
	const PS_InternalData_t &portalSimulatorData = portalSimulator.GetInternalData();
	CPortalSimulator *pLinkedPortalSimulator = portalSimulator.GetLinkedPortalSimulator();

	//bool bTraceDisplacements = sv_portal_trace_vs_displacements.GetBool();
	bool bTraceStaticProps = sv_portal_trace_vs_staticprops.GetBool();
	if( sv_portal_trace_vs_holywall.GetBool() == false )
		bTraceHolyWall = false;

	bool bTraceTransformedGeometry = ( (pLinkedPortalSimulator != NULL) && bTraceHolyWall && (portalSimulator.IsRayInPortalHole( queryRay ) != RIPHR_NOT_TOUCHING_HOLE) );	

	bool bCopyBackBrushTraceData = false;

	

	// Traces vs world
	if( pTraceFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
	{
		if( sv_portal_trace_vs_world.GetBool() )
		{
			for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( portalSimulatorData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
			{
				const PS_SD_Static_BrushSet_t *pBrushSet = &portalSimulatorData.Simulation.Static.World.Brushes.BrushSets[iBrushSet];
				if( ((pBrushSet->iSolidMask & fMask) != 0) && pBrushSet->pCollideable && 
					physcollision->TraceBoxAA( queryRay, pBrushSet->pCollideable, &TempTrace ) )
				{
					bCopyBackBrushTraceData = true;

					bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
					if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser) )
					{
						*pTrace = TempTrace;

						if ( portal_trace_shrink_ray_each_query.GetBool() )
						{
							queryRay.m_Delta *= pTrace->fraction;
						}
					}

					if ( pTrace->DidHit() && portal_trace_shrink_ray_each_query.GetBool() )
					{
						queryRay.m_Delta *= pTrace->fraction;
					}

					Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );

#if defined ( PORTAL_TRACE_LOGGING )
					keptType = BRUSHES;
					s_TraceLogger.LogTrace( BRUSHES );
#endif
				}
			}
		}

		if( bTraceStaticProps )
		{
			bool bFilterStaticProps = (pTraceFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS);
			//local clipped static props
			{
				int iLocalStaticCount = portalSimulatorData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count();
				if( iLocalStaticCount != 0 && portalSimulatorData.Simulation.Static.World.StaticProps.bCollisionExists )
				{
					const PS_SD_Static_World_StaticProps_ClippedProp_t *pCurrentProp = portalSimulatorData.Simulation.Static.World.StaticProps.ClippedRepresentations.Base();
					const PS_SD_Static_World_StaticProps_ClippedProp_t *pStop = pCurrentProp + iLocalStaticCount;
					do
					{
						if( ( !bFilterStaticProps || pTraceFilter->ShouldHitEntity( pCurrentProp->pSourceProp, fMask ) ) && 
							physcollision->TraceBoxAA( queryRay, pCurrentProp->pCollide, &TempTrace ) )
						{
#if defined ( PORTAL_TRACE_LOGGING )
							s_TraceLogger.LogTrace( STATIC_PROPS );
#endif

							bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
							if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser) )
							{
								*pTrace = TempTrace;
								pTrace->surface.flags = pCurrentProp->iTraceSurfaceFlags;
								pTrace->surface.surfaceProps = pCurrentProp->iTraceSurfaceProps;
								pTrace->surface.name = pCurrentProp->szTraceSurfaceName;
								pTrace->contents = pCurrentProp->iTraceContents;
								pTrace->m_pEnt = pCurrentProp->pTraceEntity;
								Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
								if ( portal_trace_shrink_ray_each_query.GetBool() )
								{
									queryRay.m_Delta *= pTrace->fraction;
								}
#if defined ( PORTAL_TRACE_LOGGING )
								keptType = STATIC_PROPS;
#endif
							}
						}

						++pCurrentProp;
					}
					while( pCurrentProp != pStop );
				}
			}

			if( bTraceHolyWall )
			{
				//remote clipped static props transformed into our wall space
				if( bTraceTransformedGeometry && (pTraceFilter->GetTraceType() != TRACE_WORLD_ONLY) && sv_portal_trace_vs_staticprops.GetBool() )
				{
					int iLocalStaticCount = pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Count();
					if( iLocalStaticCount != 0 )
					{
						const PS_SD_Static_World_StaticProps_ClippedProp_t *pCurrentProp = pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Base();
						const PS_SD_Static_World_StaticProps_ClippedProp_t *pStop = pCurrentProp + iLocalStaticCount;
						Vector vTransform = portalSimulatorData.Placement.ptaap_LinkedToThis.ptShrinkAlignedOrigin;
						QAngle qTransform = portalSimulatorData.Placement.ptaap_LinkedToThis.qAngleTransform;

						do
						{
							if ( !bFilterStaticProps || pTraceFilter->ShouldHitEntity( pCurrentProp->pSourceProp, fMask ) )
							{
								physcollision->TraceBox( queryRay, pCurrentProp->pCollide, vTransform, qTransform, &TempTrace );
#if defined ( PORTAL_TRACE_LOGGING )
								s_TraceLogger.LogTrace( REMOTE_STATIC_PROPS );
#endif
								bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
								if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser ) )
								{
									*pTrace = TempTrace;
									pTrace->surface.flags = pCurrentProp->iTraceSurfaceFlags;
									pTrace->surface.surfaceProps = pCurrentProp->iTraceSurfaceProps;
									pTrace->surface.name = pCurrentProp->szTraceSurfaceName;
									pTrace->contents = pCurrentProp->iTraceContents;
									pTrace->m_pEnt = pCurrentProp->pTraceEntity;
									Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
									if ( portal_trace_shrink_ray_each_query.GetBool() )
									{
										queryRay.m_Delta *= pTrace->fraction;
									}
#if defined ( PORTAL_TRACE_LOGGING )
									keptType = REMOTE_STATIC_PROPS;
#endif
								}
							}

							++pCurrentProp;
						}
						while( pCurrentProp != pStop );
					}
				}
			}
		}

		if( portalSimulatorData.Simulation.Static.World.Displacements.pCollideable && sv_portal_trace_vs_world.GetBool() && portal_clone_displacements.GetBool() )
		{
			physcollision->TraceBox( queryRay, portalSimulatorData.Simulation.Static.World.Displacements.pCollideable, vec3_origin, vec3_angle, &TempTrace );
#if defined ( PORTAL_TRACE_LOGGING )
			s_TraceLogger.LogTrace( DISP );
#endif
			bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
			if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser )  )
			{
				*pTrace = TempTrace;
				bCopyBackBrushTraceData = true;
				//pTrace->dispFlags |= DISPSURF_FLAG_SURFACE | DISPSURF_FLAG_WALKABLE;
				Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
				if ( portal_trace_shrink_ray_each_query.GetBool() )
				{
					queryRay.m_Delta *= pTrace->fraction;
				}
#if defined ( PORTAL_TRACE_LOGGING )
				keptType = DISP;
#endif
			}
		}

		if( bTraceHolyWall )
		{
			for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( portalSimulatorData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
			{
				const PS_SD_Static_BrushSet_t *pBrushSet = &portalSimulatorData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet];
				if( ((pBrushSet->iSolidMask & fMask) != 0) && pBrushSet->pCollideable && 
					physcollision->TraceBoxAA( queryRay, pBrushSet->pCollideable, &TempTrace ) )
				{
#if defined ( PORTAL_TRACE_LOGGING )
					s_TraceLogger.LogTrace( HOLYWALL_BRUSHES );
#endif
					bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
					if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser)  )
					{
						*pTrace = TempTrace;
						bCopyBackBrushTraceData = true;
						Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
						if ( portal_trace_shrink_ray_each_query.GetBool() )
						{
							queryRay.m_Delta *= pTrace->fraction;
						}
#if defined ( PORTAL_TRACE_LOGGING )
						keptType = HOLYWALL_BRUSHES;
#endif
					}				
				}
			}

			if( portalSimulatorData.Simulation.Static.Wall.Local.Tube.pCollideable )
			{
				physcollision->TraceBoxAA( queryRay, portalSimulatorData.Simulation.Static.Wall.Local.Tube.pCollideable, &TempTrace );

#if defined ( PORTAL_TRACE_LOGGING )
				s_TraceLogger.LogTrace( HOLYWALL_TUBE );
#endif
				bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
				if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser) )
				{
					*pTrace = TempTrace;
					bCopyBackBrushTraceData = true;
					if ( portal_trace_shrink_ray_each_query.GetBool() )
					{
						queryRay.m_Delta *= pTrace->fraction;
					}
#if defined ( PORTAL_TRACE_LOGGING )
					keptType = HOLYWALL_TUBE;
#endif
				}
			}

			//if( portalSimulatorData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable && sv_portal_trace_vs_world.GetBool() )
			if( bTraceTransformedGeometry )
			{
				for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
				{
					const PS_SD_Static_BrushSet_t *pBrushSet = &pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet];
					if( ((pBrushSet->iSolidMask & fMask) != 0) && pBrushSet->pCollideable )
					{
						physcollision->TraceBox( queryRay, pBrushSet->pCollideable, portalSimulatorData.Placement.ptaap_LinkedToThis.ptOriginTransform, portalSimulatorData.Placement.ptaap_LinkedToThis.qAngleTransform, &TempTrace );
#if defined ( PORTAL_TRACE_LOGGING )
						s_TraceLogger.LogTrace( HOLYWALL_TRANSLATED_BRUSHES );
#endif
						bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
						if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser )  )
						{
							*pTrace = TempTrace;
							bCopyBackBrushTraceData = true;
							if ( portal_trace_shrink_ray_each_query.GetBool() )
							{
								queryRay.m_Delta *= pTrace->fraction;
							}
							Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
#if defined ( PORTAL_TRACE_LOGGING )
							keptType = HOLYWALL_TRANSLATED_BRUSHES;
#endif
						}
					}
				}
			}
		}	

		if( bCopyBackBrushTraceData )
		{
			pTrace->surface = portalSimulatorData.Simulation.Static.SurfaceProperties.surface;
			pTrace->contents = portalSimulatorData.Simulation.Static.SurfaceProperties.contents;
			pTrace->m_pEnt = portalSimulatorData.Simulation.Static.SurfaceProperties.pEntity;

			bCopyBackBrushTraceData = false;
		}
	}
	
	// Traces vs entities
	if( pTraceFilter->GetTraceType() != TRACE_WORLD_ONLY )
	{

		//solid entities
		CPortalCollideableEnumerator enumerator( pPortal );

		int PartitionMask;
#if defined( CLIENT_DLL )
		PartitionMask = PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_STATIC_PROPS;
#else
		PartitionMask = PARTITION_ENGINE_SOLID_EDICTS | PARTITION_ENGINE_STATIC_PROPS;
#endif

		::partition->EnumerateElementsAlongRay( PartitionMask, queryRay, false, &enumerator );
		for( counter = 0; counter != enumerator.m_iHandleCount; ++counter )
		{
			if( staticpropmgr->IsStaticProp( enumerator.m_pHandles[counter] ) )
			{
				//if( bFilterStaticProps && !pTraceFilter->ShouldHitEntity( enumerator.m_pHandles[counter], fMask ) )
				continue; //static props are handled separately, with clipped versions
			}
            else if ( !pTraceFilter->ShouldHitEntity( enumerator.m_pHandles[counter], fMask ) )
			{
				continue;
			}

			CBaseEntity *pEnumeratedEntity = EntityFromEntityHandle( enumerator.m_pHandles[counter] );

			//If we have a carved representation of this entity, trace against that instead of the real thing
			CPhysCollide *pCarvedCollide = portalSimulator.IsEntityCarvedByPortal( pEnumeratedEntity ) ? portalSimulator.GetCollideForCarvedEntity( pEnumeratedEntity ) : NULL;
			if( pCarvedCollide != NULL )
			{
				ICollideable *pUncarvedCollideable = pEnumeratedEntity->GetCollideable();
				physcollision->TraceBox( queryRay, pCarvedCollide, pUncarvedCollideable->GetCollisionOrigin(), pUncarvedCollideable->GetCollisionAngles(), &TempTrace );
#if defined ( PORTAL_TRACE_LOGGING )
				s_TraceLogger.LogTrace( ENTITIES );
#endif
			
				bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
				if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser ) )
				{
					//copy the trace data from the carved trace
					*pTrace = TempTrace;

					//then trace against the real thing for surface data.
					//TODO: There's got to be a way to store this off and look it up intelligently. But I can't seem to find surface info without a trace, making the results only valid for that trace.
					enginetrace->ClipRayToEntity( queryRay, fMask, enumerator.m_pHandles[counter], &TempTrace );
					pTrace->contents = TempTrace.contents;
					pTrace->surface = TempTrace.surface;
					pTrace->m_pEnt = TempTrace.m_pEnt;
					Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
					if ( portal_trace_shrink_ray_each_query.GetBool() )
					{
						queryRay.m_Delta *= pTrace->fraction;
					}
#if defined ( PORTAL_TRACE_LOGGING )
					keptType = ENTITIES;
#endif
				}
			}
			else
			{
				enginetrace->ClipRayToEntity( queryRay, fMask, enumerator.m_pHandles[counter], &TempTrace );
#if defined ( PORTAL_TRACE_LOGGING )
				s_TraceLogger.LogTrace( ENTITIES );
#endif
				bool bIsCloser = (portal_trace_shrink_ray_each_query.GetBool() ) ? (TempTrace.DidHit()) : (TempTrace.fraction < pTrace->fraction);
				if( (TempTrace.startsolid && !pTrace->startsolid) || (queryRay.m_IsSwept && bIsCloser ) )
				{
					*pTrace = TempTrace;
					Assert( pTrace->startsolid || (pTrace->fraction == 1.0f) || (pTrace->plane.normal.LengthSqr() > 0.5f) );
					if ( portal_trace_shrink_ray_each_query.GetBool() )
					{
						queryRay.m_Delta *= pTrace->fraction;
					}
#if defined ( PORTAL_TRACE_LOGGING )
					keptType = ENTITIES;
#endif
				}
			}			
		}
	}

#if defined ( PORTAL_TRACE_LOGGING )
	if ( pTrace->DidHit() && keptType >= 0 && keptType < TRACE_TYPE_COUNT )
		s_TraceLogger.LogTypeKept( (eTraceType)keptType );

	s_TraceLogger.Display();
#endif
}

void UTIL_Portal_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall )
{
	CTraceFilterSimple traceFilter( ignore, collisionGroup );
	UTIL_Portal_TraceRay( pPortal, ray, fMask, &traceFilter, pTrace, bTraceHolyWall );
}

//-----------------------------------------------------------------------------
// Purpose: Trace a ray 'past' a portal's surface, hitting objects in the linked portal's collision environment
// Input  : *pPortal - The portal being traced 'through'
//			&ray - The ray being traced
//			fMask - trace mask to cull results
//			*pTraceFilter - trace filter to cull results
//			*pTrace - Empty trace to return the result (value will be overwritten)
//-----------------------------------------------------------------------------
void UTIL_PortalLinked_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall )
{
	// Transform the specified ray to the remote portal's space
	Ray_t rayTransformed;
	UTIL_Portal_RayTransform( pPortal->MatrixThisToLinked(), ray, rayTransformed );

	AssertMsg ( ray.m_IsRay, "Ray with extents across portal tracing not implemented!" );

	const CPortalSimulator &portalSimulator = pPortal->m_PortalSimulator;
	CPortal_Base2D *pLinkedPortal = (CPortal_Base2D*)(pPortal->m_hLinkedPortal.Get());
	if( (pLinkedPortal == NULL) || (portalSimulator.IsRayInPortalHole( ray ) == RIPHR_NOT_TOUCHING_HOLE) )
	{
		memset( pTrace, 0, sizeof(trace_t));
		pTrace->fraction = 1.0f;
		pTrace->fractionleftsolid = 0;

		pTrace->contents = pPortal->m_PortalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.contents;
		pTrace->surface  = pPortal->m_PortalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.surface;
		pTrace->m_pEnt	 = pPortal->m_PortalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.pEntity;
		return;
	}
	UTIL_Portal_TraceRay( pLinkedPortal, rayTransformed, fMask, pTraceFilter, pTrace, bTraceHolyWall );

	// Transform the ray's start, end and plane back into this portal's space, 
	// because we react to the collision as it is displayed, and the image is displayed with this local portal's orientation.
	VMatrix matLinkedToThis = pLinkedPortal->MatrixThisToLinked();
	UTIL_Portal_PointTransform( matLinkedToThis, pTrace->startpos, pTrace->startpos );
	UTIL_Portal_PointTransform( matLinkedToThis, pTrace->endpos, pTrace->endpos );
	UTIL_Portal_PlaneTransform( matLinkedToThis, pTrace->plane, pTrace->plane );
}

void UTIL_PortalLinked_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall )
{
	CTraceFilterSimple traceFilter( ignore, collisionGroup );
	UTIL_PortalLinked_TraceRay( pPortal, ray, fMask, &traceFilter, pTrace, bTraceHolyWall );
}


int UTIL_Portal_EntitiesAlongRayComplex( int *entSegmentIndices, int *segCount, int maxEntities, ComplexPortalTrace_t *pResultSegmentArray, int maxSegments, const Ray_t& ray, ICountedPartitionEnumerator* pEnum, ITraceFilter* pTraceFilter, int fStopTraceContents )
{
	if( !pEnum )
		return 0;

	CTraceFilterHitAll dummyFilter;
	if( !pTraceFilter )
		pTraceFilter = &dummyFilter;

	ComplexPortalTrace_t dummySegmentResults[16];
	if( !pResultSegmentArray )
	{
		pResultSegmentArray = dummySegmentResults;
		maxSegments = ARRAYSIZE( dummySegmentResults );
	}

	// Run a complex trace that hits only the world to get all the segments to trace with
	const int segmentCount = UTIL_Portal_ComplexTraceRay( ray, fStopTraceContents, pTraceFilter, pResultSegmentArray, maxSegments );
	if( segCount )
		*segCount = segmentCount;

	for( int i = 0; i < segmentCount && pEnum->GetCount() < maxEntities; ++i )
	{
		// Enumerate all entities along the ray
		Ray_t segmentRay;
		segmentRay.Init( pResultSegmentArray[i].trSegment.startpos, pResultSegmentArray[i].trSegment.endpos - DIST_EPSILON * pResultSegmentArray[i].trSegment.plane.normal );
		//NDebugOverlay::Line( segmentRay.m_Start, segmentRay.m_Start + segmentRay.m_Delta, 0, 255, 0, false, 5 );

		const int oldEnumCount = pEnum->GetCount();

		#ifdef GAME_DLL
		const int PARTITION_ALL_SERVER_EDICTS = PARTITION_ENGINE_NON_STATIC_EDICTS | PARTITION_ENGINE_STATIC_PROPS | PARTITION_ENGINE_SOLID_EDICTS | PARTITION_ENGINE_TRIGGER_EDICTS;
		if( segmentRay.m_Delta.IsZeroFast() )
			::partition->EnumerateElementsAtPoint( PARTITION_ALL_SERVER_EDICTS, segmentRay.m_Start, false, pEnum );
		else
			::partition->EnumerateElementsAlongRay( PARTITION_ALL_SERVER_EDICTS, segmentRay, false, pEnum );
		#else
		if( segmentRay.m_Delta.IsZeroFast() )
			::partition->EnumerateElementsAtPoint( PARTITION_ALL_CLIENT_EDICTS, segmentRay.m_Start, false, pEnum );
		else
			::partition->EnumerateElementsAlongRay( PARTITION_ALL_CLIENT_EDICTS, segmentRay, false, pEnum );
		#endif

		const int newEnumCount = pEnum->GetCount();
		const int remainingEnts = MAX( 0, maxEntities - newEnumCount );
		const int rayEnumCount = MIN( remainingEnts, newEnumCount - oldEnumCount );
		if( entSegmentIndices )
		{
			for( int j = 0; j < rayEnumCount; ++j, ++entSegmentIndices )
			{
				*entSegmentIndices = i;
			}
		}
	}

	// Add whatever stopped the trace
	CBaseEntity* pLastTraceEntity = segmentCount ? pResultSegmentArray[segmentCount - 1].trSegment.m_pEnt : NULL;
	if( pLastTraceEntity && pEnum->GetCount() < maxEntities )
	{
		const int oldCount = pEnum->GetCount();
		pEnum->EnumElement( pLastTraceEntity );
		if( pEnum->GetCount() == oldCount + 1 )
		{
			if( entSegmentIndices )
				*entSegmentIndices = segmentCount - 1;
		}
	}

	return pEnum->GetCount();
}


struct CollideableTraceData_t
{
	Vector vecAbsStart;
	Vector vecAbsEnd;
	CPhysCollide *pEntCollide;
	QAngle qCollisionAngle;
};

void TraceFunc_Collideable( void *pData, CPhysCollide *pCollide, const Vector &vOrigin, const QAngle &qAngle, trace_t *pTrace )
{
	physcollision->TraceCollide( ((CollideableTraceData_t *)pData)->vecAbsStart, ((CollideableTraceData_t *)pData)->vecAbsEnd, ((CollideableTraceData_t *)pData)->pEntCollide, ((CollideableTraceData_t *)pData)->qCollisionAngle, 
		pCollide, vOrigin, qAngle, pTrace );
}

void TraceFunc_Ray( void *pData, CPhysCollide *pCollide, const Vector &vOrigin, const QAngle &qAngle, trace_t *pTrace )
{
	physcollision->TraceBox( *(Ray_t *)pData, MASK_ALL, NULL, pCollide, vOrigin, qAngle, pTrace );
}



//-----------------------------------------------------------------------------
// Purpose: A version of trace entity which detects portals and translates the trace through portals
//-----------------------------------------------------------------------------
void UTIL_Portal_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
							 unsigned int mask, ITraceFilter *pFilter, trace_t *pTrace )
{
	if ( !pEntity || !pEntity->CollisionProp() )
	{
		Assert ( 0 );
		return;
	}

#ifdef CLIENT_DLL
	Assert( pEntity->IsPlayer() );

	CPortalSimulator *pPortalSimulator = NULL;
	if( pEntity->IsPlayer() )
	{
		C_Portal_Base2D *pPortal = ((C_Portal_Player *)pEntity)->m_hPortalEnvironment.Get();
		if( pPortal )
			pPortalSimulator = &pPortal->m_PortalSimulator;
	}
#else
	CPortalSimulator *pPortalSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
#endif

	memset( pTrace, 0, sizeof(trace_t));
	pTrace->fraction = 1.0f;
	pTrace->fractionleftsolid = 0;

	ICollideable* pCollision = enginetrace->GetCollideable( pEntity );

	// If main is simulating this object, trace as UTIL_TraceEntity would
	trace_t realTrace;
	QAngle qCollisionAngles = pCollision->GetCollisionAngles();
	// BUG: sweep collideable requires no angles to be passed in
	enginetrace->SweepCollideable( pCollision, vecAbsStart, vecAbsEnd, vec3_angle, mask, pFilter, &realTrace );

	// For the below box test, we need to add the tolerance onto the extents, because the underlying
	// box on plane side test doesn't use the parameter tolerance.
	float flTolerance = 0.1f;
	Vector vEntExtents = pEntity->CollisionProp()->OBBSize() * 0.5 + Vector ( flTolerance, flTolerance, flTolerance );
	Vector vColCenter = realTrace.endpos + ( pEntity->CollisionProp()->OBBMins() + pEntity->CollisionProp()->OBBMaxs() ) * 0.5f;

	// If this entity is not simulated in a portal environment, trace as normal
    if( pPortalSimulator == NULL )
	{
		// If main is simulating this object, trace as UTIL_TraceEntity would
		*pTrace = realTrace;
	}
	else
	{
		CPhysCollide *pTempPhysCollide = (sv_portal_new_player_trace.GetBool() && (pEntity->GetSolid() == SOLID_BBOX)) ? NULL : physcollision->BBoxToCollide( pCollision->OBBMins(), pCollision->OBBMaxs() );
		//CPhysCollide *pTempPhysCollide = physcollision->BBoxToCollide( pCollision->OBBMins(), pCollision->OBBMaxs() );
		CPhysCollide *pEntPhysCollide = pTempPhysCollide; //HACKHACK: At some point we should investigate into getting the real CPhysCollide

		void (*pTraceFunc)( void *, CPhysCollide *, const Vector &, const QAngle &, trace_t * );
		void *pTraceData;
		Ray_t entRay;
		entRay.Init( vecAbsStart, vecAbsEnd, pCollision->OBBMins(), pCollision->OBBMaxs() );
		CollideableTraceData_t entCollideableData;

		if( pEntPhysCollide )
		{
			pTraceFunc = TraceFunc_Collideable;
			pTraceData = &entCollideableData;
			entCollideableData.pEntCollide = pEntPhysCollide;
			entCollideableData.qCollisionAngle = qCollisionAngles;
			entCollideableData.vecAbsStart = vecAbsStart;
			entCollideableData.vecAbsEnd = vecAbsEnd;
		}
		else
		{
			pTraceFunc = TraceFunc_Ray;
			pTraceData = &entRay;
		}

		CPortalSimulator *pLinkedPortalSimulator = pPortalSimulator->GetLinkedPortalSimulator();

#if 0 // this trace for brush ents made sense at one time, but it's 'overcolliding' during portal transitions (bugzilla#25)
		if( realTrace.m_pEnt && (realTrace.m_pEnt->GetMoveType() != MOVETYPE_NONE) ) //started by hitting something moving which wouldn't be detected in the following traces
		{
			float fFirstPortalFraction = 2.0f;
			CPortal_Base2D *pFirstPortal = UTIL_Portal_FirstAlongRay( entRay, fFirstPortalFraction );

			if ( !pFirstPortal )
				*pTrace = realTrace;
			else
			{
				if ( pFirstPortal->m_vForward.Dot( realTrace.endpos - pFirstPortal->m_ptOrigin ) > 0.0f )
					*pTrace = realTrace;
			}
		}
#endif

		// We require both environments to be active in order to trace against them
		Assert ( pCollision );
		if ( !pCollision  )
		{
			return;
		}

		// World, displacements and holy wall are stored in separate collideables
		// Traces against each and keep the closest intersection (if any)
		trace_t tempTrace;

		// Hit the world
		if ( pFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
		{
			if( sv_portal_trace_vs_world.GetBool() )
			{
				for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
				{
					if( ((pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].iSolidMask & mask) != 0) &&
						pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
					{
						pTraceFunc( pTraceData, pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, vec3_origin, vec3_angle, &tempTrace );
						
						if ( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
						{
							*pTrace = tempTrace;
						}
					}
				}
			}

			if( pPortalSimulator->GetInternalData().Simulation.Static.World.Displacements.pCollideable && 
				sv_portal_trace_vs_world.GetBool() && 
				portal_clone_displacements.GetBool() )
			{
				pTraceFunc( pTraceData, pPortalSimulator->GetInternalData().Simulation.Static.World.Displacements.pCollideable, vec3_origin, vec3_angle, &tempTrace );
				
				if ( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
				{
					*pTrace = tempTrace;
				}
			}

			//if( pPortalSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable &&
			if( pLinkedPortalSimulator &&
				sv_portal_trace_vs_world.GetBool() && 
				sv_portal_trace_vs_holywall.GetBool() )
			{
				for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
				{
					if( ((pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].iSolidMask & mask) != 0) &&
						pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
					{
						pTraceFunc( pTraceData, pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.ptOriginTransform, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform, &tempTrace );

						if ( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
						{
							*pTrace = tempTrace;
						}
					}
				}
			}

			//if( pPortalSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable &&
			if( pLinkedPortalSimulator &&
				pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Displacements.pCollideable &&
				sv_portal_trace_vs_world.GetBool() && 
				sv_portal_trace_vs_holywall.GetBool() && 
				portal_clone_displacements.GetBool() )
			{
				pTraceFunc( pTraceData, pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Displacements.pCollideable, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.ptOriginTransform, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform, &tempTrace );

				if ( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
				{
					*pTrace = tempTrace;
				}
			}

			if ( sv_portal_trace_vs_holywall.GetBool() )
			{
				for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
				{
					if( ((pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].iSolidMask & mask) != 0) &&
						pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable )
					{
						pTraceFunc( pTraceData, pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable, vec3_origin, vec3_angle, &tempTrace );

						if ( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
						{
							if( tempTrace.fraction == 0.0f )
								tempTrace.startsolid = true;

							if( tempTrace.fractionleftsolid == 1.0f )
								tempTrace.allsolid = true;

							*pTrace = tempTrace;
						}
					}
				}
			}

			if ( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Tube.pCollideable && 
				sv_portal_trace_vs_holywall.GetBool() )
			{
				pTraceFunc( pTraceData, pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Tube.pCollideable, vec3_origin, vec3_angle, &tempTrace );

				if( (tempTrace.startsolid == false) && (tempTrace.fraction < pTrace->fraction) ) //never allow something to be stuck in the tube, it's more of a last-resort guide than a real collideable
				{
					*pTrace = tempTrace;
				}
			}

			// For all brush traces, use the 'portal backbrush' surface surface contents
			// BUGBUG: Doing this is a great solution because brushes near a portal
			// will have their contents and surface properties homogenized to the brush the portal ray hit.
			if ( pTrace->startsolid || (pTrace->fraction < 1.0f) )
			{
				pTrace->surface = pPortalSimulator->GetInternalData().Simulation.Static.SurfaceProperties.surface;
				pTrace->contents = pPortalSimulator->GetInternalData().Simulation.Static.SurfaceProperties.contents;
				pTrace->m_pEnt = pPortalSimulator->GetInternalData().Simulation.Static.SurfaceProperties.pEntity;
			}
		}

		// Trace vs entities
		if ( pFilter->GetTraceType() != TRACE_WORLD_ONLY )
		{
			if( sv_portal_trace_vs_staticprops.GetBool() && (pFilter->GetTraceType() != TRACE_ENTITIES_ONLY) )
			{
				bool bFilterStaticProps = (pFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS);
				
				//local clipped static props
				{
					int iLocalStaticCount = pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Count();
					if( iLocalStaticCount != 0 && pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.bCollisionExists )
					{
						const PS_SD_Static_World_StaticProps_ClippedProp_t *pCurrentProp = pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Base();
						const PS_SD_Static_World_StaticProps_ClippedProp_t *pStop = pCurrentProp + iLocalStaticCount;
						Vector vTransform = vec3_origin;
						QAngle qTransform = vec3_angle;

						do
						{
							if( (!bFilterStaticProps) || pFilter->ShouldHitEntity( pCurrentProp->pSourceProp, mask ) )
							{
								pTraceFunc( pTraceData, pCurrentProp->pCollide, vTransform, qTransform, &tempTrace );

								if( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
								{
									*pTrace = tempTrace;
									pTrace->surface.flags = pCurrentProp->iTraceSurfaceFlags;
									pTrace->surface.surfaceProps = pCurrentProp->iTraceSurfaceProps;
									pTrace->surface.name = pCurrentProp->szTraceSurfaceName;
									pTrace->contents = pCurrentProp->iTraceContents;
									pTrace->m_pEnt = pCurrentProp->pTraceEntity;
								}
							}

							++pCurrentProp;
						}
						while( pCurrentProp != pStop );
					}
				}

				if( pLinkedPortalSimulator && pPortalSimulator->EntityIsInPortalHole( pEntity ) )
				{

#ifndef CLIENT_DLL
					if( sv_use_transformed_collideables.GetBool() ) //if this never gets turned off, it should be removed before release
					{
						//moving entities near the remote portal
						CBaseEntity *pEnts[1024];
						int iEntCount = pLinkedPortalSimulator->GetMoveableOwnedEntities( pEnts, 1024 );

						CTransformedCollideable transformedCollideable;
						transformedCollideable.m_matTransform = pLinkedPortalSimulator->GetInternalData().Placement.matThisToLinked;
						transformedCollideable.m_matInvTransform = pLinkedPortalSimulator->GetInternalData().Placement.matLinkedToThis;
						for( int i = 0; i != iEntCount; ++i )
						{
							CBaseEntity *pRemoteEntity = pEnts[i];
							if( pRemoteEntity->GetSolid() == SOLID_NONE )
								continue;

							transformedCollideable.m_pWrappedCollideable = pRemoteEntity->GetCollideable();
							Assert( transformedCollideable.m_pWrappedCollideable != NULL );
	                        						
							//enginetrace->ClipRayToCollideable( entRay, mask, &transformedCollideable, pTrace );

							enginetrace->ClipRayToCollideable( entRay, mask, &transformedCollideable, &tempTrace );
							if( tempTrace.startsolid || (tempTrace.fraction < pTrace->fraction) )
							{
								*pTrace = tempTrace;
							}
						}
					}
#endif
				}
			}
		}

		if( pTrace->fraction == 1.0f ) 
		{
			memset( pTrace, 0, sizeof( trace_t ) );
			pTrace->fraction = 1.0f;
			pTrace->startpos = vecAbsStart;
			pTrace->endpos = vecAbsEnd;
		}

		if( pTempPhysCollide )
		{
			physcollision->DestroyCollide( pTempPhysCollide );
		}
	}	
}

void UTIL_Portal_PointTransform( const VMatrix &matThisToLinked, const Vector &ptSource, Vector &ptTransformed )
{
	ptTransformed = matThisToLinked * ptSource;
}

void UTIL_Portal_VectorTransform( const VMatrix &matThisToLinked, const Vector &vSource, Vector &vTransformed )
{
	vTransformed = matThisToLinked.ApplyRotation( vSource );
}

void UTIL_Portal_AngleTransform( const VMatrix &matThisToLinked, const QAngle &qSource, QAngle &qTransformed )
{
	qTransformed = TransformAnglesToWorldSpace( qSource, matThisToLinked.As3x4() );
}

void UTIL_Portal_RayTransform( const VMatrix &matThisToLinked, const Ray_t &raySource, Ray_t &rayTransformed )
{
	rayTransformed = raySource;

	UTIL_Portal_PointTransform( matThisToLinked, raySource.m_Start, rayTransformed.m_Start );
	UTIL_Portal_VectorTransform( matThisToLinked, raySource.m_StartOffset, rayTransformed.m_StartOffset );
	UTIL_Portal_VectorTransform( matThisToLinked, raySource.m_Delta, rayTransformed.m_Delta );

	//BUGBUG: Extents are axis aligned, so rotating it won't necessarily give us what we're expecting
	UTIL_Portal_VectorTransform( matThisToLinked, raySource.m_Extents, rayTransformed.m_Extents );
	
	//HACKHACK: Negative extents hang in traces, make each positive because we rotated it above
	if ( rayTransformed.m_Extents.x < 0.0f )
	{
		rayTransformed.m_Extents.x = -rayTransformed.m_Extents.x;
	}
	if ( rayTransformed.m_Extents.y < 0.0f )
	{
		rayTransformed.m_Extents.y = -rayTransformed.m_Extents.y;
	}
	if ( rayTransformed.m_Extents.z < 0.0f )
	{
		rayTransformed.m_Extents.z = -rayTransformed.m_Extents.z;
	}

}

struct IntersectionCachedData_t
{
	Vector vCenter;
	VPlane plane;
	Vector vUp, vRight;
	float fRadiusSquare;
};

inline CPortal_Base2D *UTIL_Portal_ClipTraceToFirstTransformingPortal( trace_t &tr, const Vector &vRayNormalizedDelta, const float fRayRadius, CPortal_Base2D **pPortals, IntersectionCachedData_t *pIntersectionData, const int iPortalCount )
{
	Vector vIntersection;
	float fIntersectionScale = FLT_MAX; //a pseudo-distance. Expressed as a scale of the existing trace length (which we never actually compute)
	CPortal_Base2D *pIntersectionPortal = NULL;

	//Portal-Ray intersection has the following axioms that allow for some cheap checks up front
	//intersection requires the ray to start in front of the portal plane and end behind it
	//intersection requires the ray to intersect a quad on the portal plane
	bool bFullTrace = tr.fraction == 1.0f;
	for( int i = 0; i != iPortalCount; ++i )
	{
		float fStartDot = pIntersectionData[i].plane.DistTo( tr.startpos );
		float fEndDot =  pIntersectionData[i].plane.DistTo( tr.endpos );

		if( fEndDot >= fStartDot ) //ray would be coming out of portal instead of going in (or didn't move at all)
			continue;

		if( (fStartDot + fRayRadius) < 0.0f )
			continue; //ray started wholly behind the portal

		if( bFullTrace && (fEndDot > (1.0f / 4096.0f)) )
			continue; //ray reached it's destination before the center crossed the portal plane

		if( (fEndDot - fRayRadius) - (1.0f / 4096.0f) > 0.01f )
			continue; //ray ended wholly in front of the portal

		//compute portal distance to the (infinite) ray
		Vector vRayStartToPortal = pIntersectionData[i].vCenter - tr.startpos;
		Vector vProjected = vRayNormalizedDelta * vRayStartToPortal.Dot( vRayNormalizedDelta );
		Vector vRayClosestToPortal = vRayStartToPortal - vProjected;

		if( vRayClosestToPortal.LengthSqr() > pIntersectionData[i].fRadiusSquare )
			continue; //portal quad is too far away for the ray's center to intersect it. This doesn't take ray extents into account because we're assuming a successful teleportation requires at least the center of the ray to intersect

		//If we're here. The ray at least grazes the portal. An intersection between the ray and this portal is very probable
		//NOTE: For rays with extents, it's possible that the end position stops short of the portal because it hit the wall behind the portal.
		float fRayIntersectionScale = fStartDot / (fStartDot - fEndDot); //how far (relative to the trace length) along the ray normal do we go before hitting the plane
		if( fRayIntersectionScale > fIntersectionScale ) //already have something closer
			continue;

		if( (fRayIntersectionScale * tr.fraction) > 1.0f )
			continue; //in the case of a ray with extents, some of the code above assumes a trace that stops short *would have* gone through, this is the actual check to verify that is the truth

		Vector vPlaneIntersection = (fRayIntersectionScale * tr.endpos) + ( (1.0f - fRayIntersectionScale) * tr.startpos ); //barycentric
		//Vector vPlaneIntersection = tr.startpos + (vRayNormalizedDelta * fRayIntersectionLength);
#ifdef DBGFLAG_ASSERT
		float fIntersectionPlaneDist = fabs( pIntersectionData[i].plane.DistTo( vPlaneIntersection ) );
		AssertOnce( fIntersectionPlaneDist <= (1.0f/256.0f) ); //arbitrary small float that's a power of 2. FLT_MIN is too small
#endif

		Vector vPortalSpacePlaneIntersection = vPlaneIntersection - pIntersectionData[i].vCenter; //move our calculation space to portal centric to make the following math simpler
		if( fabs( pIntersectionData[i].vRight.Dot( vPortalSpacePlaneIntersection ) ) > pPortals[i]->GetHalfWidth() )
			continue; //ray center outside of portal quad

		if( fabs( pIntersectionData[i].vUp.Dot( vPortalSpacePlaneIntersection ) ) > pPortals[i]->GetHalfHeight() )
			continue; //ray center outside of portal quad

		//HIT! You sank my portalship. Don't know if the entire ray intersects, but at least the center does

		//which is good enough for now. TODO: Test forwardmost corners as well?
		AssertMsgOnce( fRayRadius < 20.0f, "This code is currently designed to assume the traced rays have small extents" );
		fIntersectionScale = fRayIntersectionScale;
		vIntersection = vPlaneIntersection;
		pIntersectionPortal = pPortals[i];
	}

	if( pIntersectionPortal != NULL )
	{
		//clip the trace to closest
		tr.endpos = vIntersection;
		tr.fraction *= fIntersectionScale;
		tr.m_pEnt = pIntersectionPortal;
		return pIntersectionPortal;
	}

	return NULL;
}

int UTIL_Portal_ComplexTraceRay( const Ray_t &ray, unsigned int mask, ITraceFilter *pTraceFilter, ComplexPortalTrace_t *pResultSegmentArray, int iMaxSegments )
{
	if( (pResultSegmentArray == NULL) || (iMaxSegments <= 0) )
		return 0;

	if( !ray.m_IsSwept )
	{
		Assert( 0 ); //shame on you for wasting our time
		return 0;
	}

	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	CPortal_Base2D **pPortals = pPortals = (CPortal_Base2D **)stackalloc( sizeof( CPortal_Base2D * ) * iPortalCount );
	if( iPortalCount != 0 )
	{
		//we only care about active/linked portals, so reduce the list to that set		
		CPortal_Base2D **pAllPortals = CPortal_Base2D_Shared::AllPortals.Base();
		int iWriteIndex = 0;
		for( int i = 0; i != iPortalCount; ++i )
		{
			if( pAllPortals[i]->IsActivedAndLinked() )
			{
				pPortals[iWriteIndex++] = pAllPortals[i];
			}
		}

		iPortalCount = iWriteIndex;
	}

	pResultSegmentArray[0].pSegmentStartPortal = NULL;

	if( iPortalCount == 0 )
	{
		//trivial case where it's impossible that it hit any portals
		pResultSegmentArray[0].pSegmentEndPortal = NULL;
		pResultSegmentArray[0].vNormalizedDelta = ray.m_Delta.Normalized();
		enginetrace->TraceRay( ray, mask, pTraceFilter, &pResultSegmentArray[0].trSegment );
		return 1;
	}

	//cache/compute some data to speed up further tests
	IntersectionCachedData_t *pIntersectionData = (IntersectionCachedData_t *)stackalloc( sizeof( IntersectionCachedData_t ) * iPortalCount );
	for( int i = 0; i != iPortalCount; ++i )
	{
		pIntersectionData[i].vCenter = pPortals[i]->m_ptOrigin;
		pIntersectionData[i].plane.Init( pPortals[i]->m_plane_Origin.normal, pPortals[i]->m_plane_Origin.dist );
		pIntersectionData[i].vRight = pPortals[i]->m_vRight;
		pIntersectionData[i].vUp = pPortals[i]->m_vUp;
		
		float x,y;
		x = pPortals[i]->GetHalfWidth();
		y = pPortals[i]->GetHalfHeight();
		pIntersectionData[i].fRadiusSquare = ( (x * x) + (y * y) );
	}

	float fRayRadius = ray.m_Extents.Length();
	float fRemainingLength = ray.m_Delta.Length(); //the ray will continually get shorter as segments use up the original delta
	Vector vRayNormalizedDelta = ray.m_Delta * (1.0f / fRemainingLength);
	Ray_t workRay = ray;

	pResultSegmentArray[0].vNormalizedDelta = vRayNormalizedDelta;

	enginetrace->TraceRay( workRay, mask, pTraceFilter, &pResultSegmentArray[0].trSegment );
	pResultSegmentArray[0].pSegmentEndPortal = UTIL_Portal_ClipTraceToFirstTransformingPortal( pResultSegmentArray[0].trSegment, vRayNormalizedDelta, fRayRadius, pPortals, pIntersectionData, iPortalCount );
	
	for( int iSegmentIndex = 1; iSegmentIndex < iMaxSegments; ++iSegmentIndex )
	{
		CPortal_Base2D *pTransformPortal = pResultSegmentArray[iSegmentIndex - 1].pSegmentEndPortal;
		if( pTransformPortal == NULL )
			return iSegmentIndex;

		fRemainingLength -= (fRemainingLength * pResultSegmentArray[iSegmentIndex - 1].trSegment.fraction);
		if( fRemainingLength <= 0.0f )
			return iSegmentIndex;

		const VMatrix &transformMatrix = pTransformPortal->MatrixThisToLinked();		
		vRayNormalizedDelta = transformMatrix.ApplyRotation( vRayNormalizedDelta );
		workRay.m_Start = (transformMatrix * pResultSegmentArray[iSegmentIndex - 1].trSegment.endpos) + (vRayNormalizedDelta * 0.5f); //NOTE: if extents are non-zero. It's possible this will start in solid
		workRay.m_Delta = vRayNormalizedDelta * fRemainingLength;
		pResultSegmentArray[iSegmentIndex].vNormalizedDelta = vRayNormalizedDelta;

		pResultSegmentArray[iSegmentIndex].pSegmentStartPortal = pTransformPortal->m_hLinkedPortal.Get();

		enginetrace->TraceRay( workRay, mask, pTraceFilter, &pResultSegmentArray[iSegmentIndex].trSegment );
		pResultSegmentArray[iSegmentIndex].pSegmentEndPortal = UTIL_Portal_ClipTraceToFirstTransformingPortal( pResultSegmentArray[iSegmentIndex].trSegment, vRayNormalizedDelta, fRayRadius, pPortals, pIntersectionData, iPortalCount );	
	}

	return iMaxSegments;
}

void UTIL_Portal_PlaneTransform( const VMatrix &matThisToLinked, const cplane_t &planeSource, cplane_t &planeTransformed )
{
	planeTransformed = planeSource;

	Vector vTrans;
	UTIL_Portal_VectorTransform( matThisToLinked, planeSource.normal, planeTransformed.normal );
	planeTransformed.dist = planeSource.dist * DotProduct( planeTransformed.normal, planeTransformed.normal );
	planeTransformed.dist += DotProduct( planeTransformed.normal, matThisToLinked.GetTranslation( vTrans ) );
}

void UTIL_Portal_PlaneTransform( const VMatrix &matThisToLinked, const VPlane &planeSource, VPlane &planeTransformed )
{
	Vector vTranformedNormal;
	float fTransformedDist;

	Vector vTrans;
	UTIL_Portal_VectorTransform( matThisToLinked, planeSource.m_Normal, vTranformedNormal );
	fTransformedDist = planeSource.m_Dist * DotProduct( vTranformedNormal, vTranformedNormal );
	fTransformedDist += DotProduct( vTranformedNormal, matThisToLinked.GetTranslation( vTrans ) );

	planeTransformed.Init( vTranformedNormal, fTransformedDist );
}


ConVar portal_triangles_overlap("portal_triangles_overlap", "0.1f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);
void UTIL_Portal_Triangles( const Vector &ptPortalCenter, const QAngle &qPortalAngles, float fHalfWidth, float fHalfHeight, Vector pvTri1[ 3 ], Vector pvTri2[ 3 ] )
{
	// Get points to make triangles
	Vector vRight, vUp;
	AngleVectors( qPortalAngles, NULL, &vRight, &vUp );

	Vector vTopEdge = vUp * fHalfHeight;
	Vector vBottomEdge = -vTopEdge;
	Vector vRightEdge = vRight * fHalfWidth;
	Vector vLeftEdge = -vRightEdge;

	Vector vTopLeft = ptPortalCenter + vTopEdge + vLeftEdge;
	Vector vTopRight = ptPortalCenter + vTopEdge + vRightEdge;
	Vector vBottomLeft = ptPortalCenter + vBottomEdge + vLeftEdge;
	Vector vBottomRight = ptPortalCenter + vBottomEdge + vRightEdge;

	// Make triangles
	float flOverlap = portal_triangles_overlap.GetFloat();
	if ( flOverlap != 0.f )
	{
		pvTri1[ 0 ] = vTopRight + flOverlap * vRight;
		pvTri1[ 1 ] = vTopLeft;
		pvTri1[ 2 ] = vBottomLeft - flOverlap * vUp;

		pvTri2[ 0 ] = vTopRight + flOverlap * vUp;
		pvTri2[ 1 ] = vBottomLeft - flOverlap * vRight;
		pvTri2[ 2 ] = vBottomRight;
	}
	else
	{
		pvTri1[ 0 ] = vTopRight;
		pvTri1[ 1 ] = vTopLeft;
		pvTri1[ 2 ] = vBottomLeft;

		pvTri2[ 0 ] = vTopRight;
		pvTri2[ 1 ] = vBottomLeft;
		pvTri2[ 2 ] = vBottomRight;
	}
}

void UTIL_Portal_Triangles( const CPortal_Base2D *pPortal, Vector pvTri1[ 3 ], Vector pvTri2[ 3 ] )
{
	UTIL_Portal_Triangles( pPortal->m_ptOrigin, pPortal->m_qAbsAngle, pPortal->GetHalfWidth(), pPortal->GetHalfHeight(), pvTri1, pvTri2 );
}

float UTIL_Portal_DistanceThroughPortal( const CPortal_Base2D *pPortal, const Vector &vPoint1, const Vector &vPoint2 )
{
	return FastSqrt( UTIL_Portal_DistanceThroughPortalSqr( pPortal, vPoint1, vPoint2 ) );
}

float UTIL_Portal_DistanceThroughPortalSqr( const CPortal_Base2D *pPortal, const Vector &vPoint1, const Vector &vPoint2 )
{
	if ( !pPortal || !pPortal->IsActive() )
		return -1.0f;

	CPortal_Base2D *pPortalLinked = pPortal->m_hLinkedPortal;
	if ( !pPortalLinked || !pPortalLinked->IsActive() )
		return -1.0f;

	return vPoint1.DistToSqr( pPortal->m_ptOrigin ) + ((Vector)pPortalLinked->m_ptOrigin).DistToSqr( vPoint2 );
}

float UTIL_Portal_ShortestDistance( const Vector &vPoint1, const Vector &vPoint2, CPortal_Base2D **pShortestDistPortal_Out /*= NULL*/, bool bRequireStraightLine /*= false*/ )
{
	return FastSqrt( UTIL_Portal_ShortestDistanceSqr( vPoint1, vPoint2, pShortestDistPortal_Out, bRequireStraightLine ) );
}

float UTIL_Portal_ShortestDistanceSqr( const Vector &vPoint1, const Vector &vPoint2, CPortal_Base2D **pShortestDistPortal_Out /*= NULL*/, bool bRequireStraightLine /*= false*/ )
{
	float fMinDist = vPoint1.DistToSqr( vPoint2 );	
	
	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
	{
		if( pShortestDistPortal_Out )
			*pShortestDistPortal_Out = NULL;

		return fMinDist;
	}
	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	CPortal_Base2D *pShortestDistPortal = NULL;

	for( int i = 0; i != iPortalCount; ++i )
	{
		CPortal_Base2D *pTempPortal = pPortals[i];
		if( pTempPortal->IsActive() )
		{
			CPortal_Base2D *pLinkedPortal = pTempPortal->m_hLinkedPortal.Get();
			if( pLinkedPortal != NULL )
			{
				Vector vPoint1Transformed = pTempPortal->MatrixThisToLinked() * vPoint1;

				float fDirectDist = vPoint1Transformed.DistToSqr( vPoint2 );
				if( fDirectDist < fMinDist )
				{
					//worth investigating further
					//find out if it's a straight line through the portal, or if we have to wrap around a corner
					float fPoint1TransformedDist = pLinkedPortal->m_plane_Origin.normal.Dot( vPoint1Transformed ) - pLinkedPortal->m_plane_Origin.dist;
					float fPoint2Dist = pLinkedPortal->m_plane_Origin.normal.Dot( vPoint2 ) - pLinkedPortal->m_plane_Origin.dist;

					bool bStraightLine = true;
					if( (fPoint1TransformedDist > 0.0f) || (fPoint2Dist < 0.0f) ) //straight line through portal impossible, part of the line has to backtrack to get to the portal surface
						bStraightLine = false;

					if( bStraightLine ) //if we're not already doing some crazy wrapping, find an intersection point
					{
						float fTotalDist = fPoint2Dist - fPoint1TransformedDist; //fPoint1TransformedDist is known to be negative
						Vector ptPlaneIntersection;

						if( fTotalDist != 0.0f )
						{
							float fInvTotalDist = 1.0f / fTotalDist;
							ptPlaneIntersection = (vPoint1Transformed * (fPoint2Dist * fInvTotalDist)) + (vPoint2 * ((-fPoint1TransformedDist) * fInvTotalDist));
						}
						else
						{
							ptPlaneIntersection = vPoint1Transformed;
						}
						
						Vector ptLinkedCenter = pLinkedPortal->m_ptOrigin;
						Vector vCenterToIntersection = ptPlaneIntersection - ptLinkedCenter;
						float fRight = pLinkedPortal->m_vRight.Dot( vCenterToIntersection );
						float fUp = pLinkedPortal->m_vUp.Dot( vCenterToIntersection );

						float fAbsRight = fabs( fRight );
						float fAbsUp = fabs( fUp );
						if( (fAbsRight > pTempPortal->GetHalfWidth()) ||
							(fAbsUp > pTempPortal->GetHalfHeight()) )
							bStraightLine = false;

						if( bStraightLine == false )
						{
							if( bRequireStraightLine )
								continue;

							//find the offending extent and shorten both extents to bring it into the portal quad
							float fNormalizer;
							if( fAbsRight > pTempPortal->GetHalfWidth() )
							{
								fNormalizer = fAbsRight/pTempPortal->GetHalfWidth();

								if( fAbsUp > pTempPortal->GetHalfHeight() )
								{
									float fUpNormalizer = fAbsUp/pTempPortal->GetHalfHeight();
									if( fUpNormalizer > fNormalizer )
										fNormalizer = fUpNormalizer;
								}
							}
							else
							{
								fNormalizer = fAbsUp/pTempPortal->GetHalfHeight();
							}

							vCenterToIntersection *= (1.0f/fNormalizer);
							ptPlaneIntersection = ptLinkedCenter + vCenterToIntersection;

							float fWrapDist = vPoint1Transformed.DistToSqr( ptPlaneIntersection ) + vPoint2.DistToSqr( ptPlaneIntersection );
							if( fWrapDist < fMinDist )
							{
								fMinDist = fWrapDist;
								pShortestDistPortal = pTempPortal;
								*pShortestDistPortal_Out = pShortestDistPortal;
							}
						}
						else
						{
							//it's a straight shot from point 1 to 2 through the portal
							fMinDist = fDirectDist;
							pShortestDistPortal = pTempPortal;
							*pShortestDistPortal_Out = pShortestDistPortal;
						}
					}
					else
					{
						if( bRequireStraightLine )
							continue;

						//do some crazy wrapped line intersection algorithm

						//for now, just do the cheap and easy solution
						float fWrapDist = vPoint1.DistToSqr( pTempPortal->m_ptOrigin ) + ((Vector)pLinkedPortal->m_ptOrigin).DistToSqr( vPoint2 );
						if( fWrapDist < fMinDist )
						{
							fMinDist = fWrapDist;
							pShortestDistPortal = pTempPortal;
							*pShortestDistPortal_Out = pShortestDistPortal;
						}
					}
				}
			}
		}
	}

	return fMinDist;
}

void UTIL_Portal_VectorToGlobalTransforms( const Vector &vPoint, CUtlVector< Vector > *utlVecPositions )
{

	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
	{
		return;
	}
	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();

	for( int i = 0; i != iPortalCount; ++i )
	{
		CPortal_Base2D *pTempPortal = pPortals[i];
		if( pTempPortal->IsActive() )
		{
			CPortal_Base2D *pLinkedPortal = pTempPortal->m_hLinkedPortal.Get();
			if( pLinkedPortal != NULL )
			{
				VMatrix matrixFromPortal;
				MatrixInverseTR( pTempPortal->MatrixThisToLinked(), matrixFromPortal );

				Vector vPoint1Transformed = matrixFromPortal * vPoint;
				utlVecPositions->AddToTail( vPoint1Transformed );
// 				vPositions[i][0] = vPoint1Transformed[0];
// 				vPositions[i][1] = vPoint1Transformed[1];
// 				vPositions[i][2] = vPoint1Transformed[2];
			}
		}
	}
}

void UTIL_Portal_AABB( const CPortal_Base2D *pPortal, Vector &vMin, Vector &vMax )
{
	Vector vOrigin = pPortal->m_ptOrigin;
	QAngle qAngles = pPortal->m_qAbsAngle;

	Vector vOBBForward;
	Vector vOBBRight;
	Vector vOBBUp;

	AngleVectors( qAngles, &vOBBForward, &vOBBRight, &vOBBUp );

	//scale the extents to usable sizes
	vOBBForward *= PORTAL_HALF_DEPTH;
	vOBBRight *= pPortal->GetHalfWidth();
	vOBBUp *= pPortal->GetHalfHeight();

	vOrigin -= vOBBForward + vOBBRight + vOBBUp;

	vOBBForward *= 2.0f;
	vOBBRight *= 2.0f;
	vOBBUp *= 2.0f;

	vMin = vMax = vOrigin;

	for( int i = 1; i != 8; ++i )
	{
		Vector ptTest = vOrigin;
		if( i & (1 << 0) ) ptTest += vOBBForward;
		if( i & (1 << 1) ) ptTest += vOBBRight;
		if( i & (1 << 2) ) ptTest += vOBBUp;

		if( ptTest.x < vMin.x ) vMin.x = ptTest.x;
		if( ptTest.y < vMin.y ) vMin.y = ptTest.y;
		if( ptTest.z < vMin.z ) vMin.z = ptTest.z;
		if( ptTest.x > vMax.x ) vMax.x = ptTest.x;
		if( ptTest.y > vMax.y ) vMax.y = ptTest.y;
		if( ptTest.z > vMax.z ) vMax.z = ptTest.z;
	}
}

float UTIL_IntersectRayWithPortal( const Ray_t &ray, const CPortal_Base2D *pPortal )
{
	if ( !pPortal || !pPortal->IsActive() )
	{
		return -1.0f;
	}

	// Discount rays not coming from the front of the portal
	float fDot = DotProduct( pPortal->m_vForward, ray.m_Delta );
	if ( fDot > 0.0f  )
	{
		return -1.0f;
	}

	Vector pvTri1[ 3 ], pvTri2[ 3 ];

	UTIL_Portal_Triangles( pPortal, pvTri1, pvTri2 );

	float fT;

	// Test triangle 1
	fT = IntersectRayWithTriangle( ray, pvTri1[ 0 ], pvTri1[ 1 ], pvTri1[ 2 ], false );

	// If there was an intersection return the T
	if ( fT >= 0.0f )
		return fT;

	// Return the result of collision with the other face triangle
	return IntersectRayWithTriangle( ray, pvTri2[ 0 ], pvTri2[ 1 ], pvTri2[ 2 ], false );
}

bool UTIL_IntersectRayWithPortalOBB( const CPortal_Base2D *pPortal, const Ray_t &ray, trace_t *pTrace )
{
	return IntersectRayWithOBB( ray, pPortal->m_ptOrigin, pPortal->m_qAbsAngle, pPortal->GetLocalMins(), pPortal->GetLocalMaxs(), 0.0f, pTrace );
}

bool UTIL_IntersectRayWithPortalOBBAsAABB( const CPortal_Base2D *pPortal, const Ray_t &ray, trace_t *pTrace )
{
	Vector vAABBMins, vAABBMaxs;

	UTIL_Portal_AABB( pPortal, vAABBMins, vAABBMaxs );

	return IntersectRayWithBox( ray, vAABBMins, vAABBMaxs, 0.0f, pTrace );
}

bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const Vector &ptPortalCenter, const QAngle &qPortalAngles, float fPortalHalfWidth, float fPortalHalfHeight, float flTolerance )
{
	Vector pvTri1[ 3 ], pvTri2[ 3 ];

	UTIL_Portal_Triangles( ptPortalCenter, qPortalAngles, fPortalHalfWidth, fPortalHalfHeight, pvTri1, pvTri2 );

	cplane_t plane;

	ComputeTrianglePlane( pvTri1[ 0 ], pvTri1[ 1 ], pvTri1[ 2 ], plane.normal, plane.dist );
	plane.type = PLANE_ANYZ;
	plane.signbits = SignbitsForPlane( &plane );

	if ( IsBoxIntersectingTriangle( vecBoxCenter, vecBoxExtents, pvTri1[ 0 ], pvTri1[ 1 ], pvTri1[ 2 ], plane, flTolerance ) )
	{
		return true;
	}

	ComputeTrianglePlane( pvTri2[ 0 ], pvTri2[ 1 ], pvTri2[ 2 ], plane.normal, plane.dist );
	plane.type = PLANE_ANYZ;
	plane.signbits = SignbitsForPlane( &plane );

	return IsBoxIntersectingTriangle( vecBoxCenter, vecBoxExtents, pvTri2[ 0 ], pvTri2[ 1 ], pvTri2[ 2 ], plane, flTolerance );
}

bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const CPortal_Base2D *pPortal, float flTolerance )
{
	if( pPortal == NULL )
		return false;

	return UTIL_IsBoxIntersectingPortal( vecBoxCenter, vecBoxExtents, pPortal->m_ptOrigin, pPortal->m_qAbsAngle, pPortal->GetHalfWidth(), pPortal->GetHalfHeight(), flTolerance );
}

CPortal_Base2D *UTIL_IntersectEntityExtentsWithPortal( const CBaseEntity *pEntity )
{
	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return NULL;

	Vector vMin, vMax;
	pEntity->CollisionProp()->WorldSpaceAABB( &vMin, &vMax );
	Vector ptCenter = ( vMin + vMax ) * 0.5f;
	Vector vExtents = ( vMax - vMin ) * 0.5f;

	CPortal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
	for( int i = 0; i != iPortalCount; ++i )
	{
		CPortal_Base2D *pTempPortal = pPortals[i];
		if( pTempPortal->IsActive() && 
			(pTempPortal->m_hLinkedPortal.Get() != NULL) &&
			UTIL_IsBoxIntersectingPortal( ptCenter, vExtents, pTempPortal )	)
		{
			return pPortals[i];
		}
	}

	return NULL;
}

void UTIL_Portal_NDebugOverlay( const Vector &ptPortalCenter, const QAngle &qPortalAngles, float fHalfWidth, float fHalfHeight, int r, int g, int b, int a, bool noDepthTest, float duration )
{
#ifndef CLIENT_DLL
	Vector pvTri1[ 3 ], pvTri2[ 3 ];

	UTIL_Portal_Triangles( ptPortalCenter, qPortalAngles, fHalfWidth, fHalfHeight, pvTri1, pvTri2 );

	NDebugOverlay::Triangle( pvTri1[ 0 ], pvTri1[ 1 ], pvTri1[ 2 ], r, g, b, a, noDepthTest, duration );
	NDebugOverlay::Triangle( pvTri2[ 0 ], pvTri2[ 1 ], pvTri2[ 2 ], r, g, b, a, noDepthTest, duration );
#endif //#ifndef CLIENT_DLL
}

void UTIL_Portal_NDebugOverlay( const CPortal_Base2D *pPortal, int r, int g, int b, int a, bool noDepthTest, float duration )
{
#ifndef CLIENT_DLL
	UTIL_Portal_NDebugOverlay( pPortal->m_ptOrigin, pPortal->m_qAbsAngle, pPortal->GetHalfWidth(), pPortal->GetHalfHeight(), r, g, b, a, noDepthTest, duration );
#endif //#ifndef CLIENT_DLL
}


struct FindClosestPassableSpace_TraceAdapter_Portal_t : public FindClosestPassableSpace_TraceAdapter_t
{
	const CPortal_Base2D *pPortal;
};

static void PortalTraceFunc( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	UTIL_Portal_TraceRay( ((FindClosestPassableSpace_TraceAdapter_Portal_t *)pTraceAdapter)->pPortal, ray, pTraceAdapter->fMask, pTraceAdapter->pTraceFilter, pResult, true );
}

static bool PortalPointOutsideWorldFunc( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	trace_t tr;
	Ray_t testRay;
	testRay.Init( vTest, vTest );

	CTraceFilterWorldOnly traceFilter;

	UTIL_Portal_TraceRay( ((FindClosestPassableSpace_TraceAdapter_Portal_t *)pTraceAdapter)->pPortal, testRay, MASK_SOLID_BRUSHONLY, &traceFilter, &tr );
	return tr.startsolid;
}

bool UTIL_FindClosestPassableSpace_InPortal( const CPortal_Base2D *pPortal, const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut )
{
	FindClosestPassableSpace_TraceAdapter_Portal_t adapter;
	adapter.pTraceFunc = PortalTraceFunc;
	adapter.pPointOutsideWorldFunc = PortalPointOutsideWorldFunc;
	adapter.pTraceFilter = pTraceFilter;
	adapter.fMask = fMask;

	adapter.pPortal = pPortal;

	return UTIL_FindClosestPassableSpace( vCenter, vExtents, vIndecisivePush, iIterations, vCenterOut, FL_AXIS_DIRECTION_NONE, &adapter );
}


struct FindClosestPassableSpace_TraceAdapter_Portal_StayInFront_t : FindClosestPassableSpace_TraceAdapter_Portal_t
{
	VPlane shiftedPlane;//we only want the center of the starting box to stay in front, not every trace. So we create a plane of solidity that is not necessarily coplanar with the portal quad
	Vector vExtentSigns; //when testing planar distance we need to use the extent closest to the solidity plane, multiply the ray extents by these to get that point.
};

static const Ray_t *AdjustRayToPlane( const Ray_t &originalRay, Ray_t &alterableRay, const Vector &vExtentSigns, const VPlane &Plane, float &fDeltaScale )
{
	Vector vExtentShift;
	vExtentShift.x = originalRay.m_Extents.x * vExtentSigns.x;
	vExtentShift.y = originalRay.m_Extents.y * vExtentSigns.y;
	vExtentShift.z = originalRay.m_Extents.z * vExtentSigns.z;
	float fExtentShiftDist = Plane.m_Normal.Dot( vExtentShift );

	if( (Plane.DistTo( originalRay.m_Start ) + fExtentShiftDist) < 0.0f )
	{
		//start solid
		NULL;
	}
	else if( (Plane.DistTo( originalRay.m_Start + originalRay.m_Delta ) + fExtentShiftDist) < 0.0f )
	{
		//end will be behind shifted plane, shorten the delta now, then expand the results on the tail end
		alterableRay = originalRay;

		float fEndDist = (Plane.DistTo( originalRay.m_Start + originalRay.m_Delta ) + fExtentShiftDist);
		float fDeltaLength = originalRay.m_Delta.Length();
		Vector vDeltaNormalized = originalRay.m_Delta / fDeltaLength;
		float fNewDeltaLength = fDeltaLength - (Plane.m_Normal.Dot( vDeltaNormalized ) * fEndDist);
		alterableRay.m_Delta = vDeltaNormalized * fNewDeltaLength;

		fDeltaScale = fNewDeltaLength / fDeltaLength;
		return &alterableRay;
	}
	else
	{
		fDeltaScale = 1.0f;
	}
	return &originalRay;
}

static void PortalTraceFunc_CenterMustStayInFront( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	const FindClosestPassableSpace_TraceAdapter_Portal_StayInFront_t *pCastedData = (const FindClosestPassableSpace_TraceAdapter_Portal_StayInFront_t *)pTraceAdapter;
	
	float fDeltaScale;
	Ray_t alterableRay;
	const Ray_t *pUseRay = AdjustRayToPlane( ray, alterableRay, pCastedData->vExtentSigns, pCastedData->shiftedPlane, fDeltaScale );
	if( pUseRay == NULL )
	{
		//start solid
		UTIL_ClearTrace( *pResult );
		pResult->startsolid = true;
		pResult->fraction = 0.0f;
		pResult->allsolid = true; //TODO: bother calculating if it leaves solid?
		return;
	}

	UTIL_Portal_TraceRay( pCastedData->pPortal, *pUseRay, pTraceAdapter->fMask, pTraceAdapter->pTraceFilter, pResult, true );

	if( pUseRay == &alterableRay )
	{
		//fixup any abnormalities from using a shortened ray
		if( !pResult->DidHit() )
		{
			pResult->m_pEnt = (CPortal_Base2D *)pCastedData->pPortal;
			pResult->plane	= pCastedData->pPortal->m_plane_Origin;
			pResult->plane.dist = pCastedData->shiftedPlane.m_Dist;
		}
		
		pResult->fraction *= fDeltaScale;
		pResult->fractionleftsolid *= fDeltaScale;		
	}
}

static bool PortalPointOutsideWorldFunc_CenterMustStayInFront( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	const FindClosestPassableSpace_TraceAdapter_Portal_StayInFront_t *pCastedData = (const FindClosestPassableSpace_TraceAdapter_Portal_StayInFront_t *)pTraceAdapter;
	if( pCastedData->shiftedPlane.DistTo( vTest ) < 0.0f )
		return true;

	return PortalPointOutsideWorldFunc( vTest, pTraceAdapter );
}

bool UTIL_FindClosestPassableSpace_InPortal_CenterMustStayInFront( const CPortal_Base2D *pPortal, const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut )
{
	FindClosestPassableSpace_TraceAdapter_Portal_StayInFront_t adapter;
	adapter.pTraceFunc = PortalTraceFunc_CenterMustStayInFront;
	adapter.pPointOutsideWorldFunc = PortalPointOutsideWorldFunc_CenterMustStayInFront;
	adapter.pTraceFilter = pTraceFilter;
	adapter.fMask = fMask;

	adapter.pPortal = pPortal;

	adapter.vExtentSigns.x = -Sign( pPortal->m_plane_Origin.normal.x );
	adapter.vExtentSigns.y = -Sign( pPortal->m_plane_Origin.normal.y );
	adapter.vExtentSigns.z = -Sign( pPortal->m_plane_Origin.normal.z );
	
	//when caclulating the shift plane, all we need to be sure of is that the most penetrating extent is coplanar with the shift plane when the center would be coplanar with the original plane
	Vector vCoplanarExtent;
	vCoplanarExtent.x = vExtents.x * adapter.vExtentSigns.x;
	vCoplanarExtent.y = vExtents.y * adapter.vExtentSigns.y;
	vCoplanarExtent.z = vExtents.z * adapter.vExtentSigns.z;

	adapter.shiftedPlane.m_Normal = pPortal->m_plane_Origin.normal;
	adapter.shiftedPlane.m_Dist = pPortal->m_plane_Origin.dist + (pPortal->m_plane_Origin.normal.Dot( vCoplanarExtent ) ); //the dot is known to be negative, shifting the plane back so the extent is coplanar with it.
	
	return UTIL_FindClosestPassableSpace( vCenter, vExtents, vIndecisivePush, iIterations, vCenterOut, FL_AXIS_DIRECTION_NONE, &adapter );
}



struct FindClosestPassableSpace_TraceAdapter_Engine_StayInFront_t : FindClosestPassableSpace_TraceAdapter_t
{
	VPlane shiftedPlane;//we only want the center of the starting box to stay in front, not every trace. So we create a plane of solidity that is not necessarily coplanar with the portal quad
	Vector vExtentSigns; //when testing planar distance we need to use the extent closest to the solidity plane, multiply the ray extents by these to get that point.
};


static void EngineTraceFunc_CenterMustStayInFront( const Ray_t &ray, trace_t *pResult, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	const FindClosestPassableSpace_TraceAdapter_Engine_StayInFront_t *pCastedData = (const FindClosestPassableSpace_TraceAdapter_Engine_StayInFront_t *)pTraceAdapter;

	float fDeltaScale;
	Ray_t alterableRay;
	const Ray_t *pUseRay = AdjustRayToPlane( ray, alterableRay, pCastedData->vExtentSigns, pCastedData->shiftedPlane, fDeltaScale );
	if( pUseRay == NULL )
	{
		//start solid
		UTIL_ClearTrace( *pResult );
		pResult->startsolid = true;
		pResult->fraction = 0.0f;
		pResult->allsolid = true; //TODO: bother calculating if it leaves solid?
		return;
	}

	enginetrace->TraceRay( *pUseRay, pTraceAdapter->fMask, pTraceAdapter->pTraceFilter, pResult );

	if( pUseRay == &alterableRay )
	{
		//fixup any abnormalities from using a shortened ray
		if( !pResult->DidHit() )
		{
#ifdef GAME_DLL
			pResult->m_pEnt = GetWorldEntity();
#else
			pResult->m_pEnt = GetClientWorldEntity();
#endif
			pResult->plane.normal = pCastedData->shiftedPlane.m_Normal;
			pResult->plane.dist = pCastedData->shiftedPlane.m_Dist;
		}

		pResult->fraction *= fDeltaScale;
		pResult->fractionleftsolid *= fDeltaScale;		
	}
}

static bool EnginePointOutsideWorldFunc_CenterMustStayInFront( const Vector &vTest, FindClosestPassableSpace_TraceAdapter_t *pTraceAdapter )
{
	const FindClosestPassableSpace_TraceAdapter_Engine_StayInFront_t *pCastedData = (const FindClosestPassableSpace_TraceAdapter_Engine_StayInFront_t *)pTraceAdapter;
	if( pCastedData->shiftedPlane.DistTo( vTest ) < 0.0f )
		return true;

	return enginetrace->PointOutsideWorld( vTest );
}

bool UTIL_FindClosestPassableSpace_CenterMustStayInFrontOfPlane( const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, const VPlane &stayInFrontOfPlane )
{
	FindClosestPassableSpace_TraceAdapter_Engine_StayInFront_t adapter;
	adapter.pTraceFunc = EngineTraceFunc_CenterMustStayInFront;
	adapter.pPointOutsideWorldFunc = EnginePointOutsideWorldFunc_CenterMustStayInFront;
	adapter.pTraceFilter = pTraceFilter;
	adapter.fMask = fMask;

	adapter.vExtentSigns.x = -Sign( stayInFrontOfPlane.m_Normal.x );
	adapter.vExtentSigns.y = -Sign( stayInFrontOfPlane.m_Normal.y );
	adapter.vExtentSigns.z = -Sign( stayInFrontOfPlane.m_Normal.z );

	//when caclulating the shift plane, all we need to be sure of is that the most penetrating extent is coplanar with the shift plane when the center would be coplanar with the original plane
	Vector vCoplanarExtent;
	vCoplanarExtent.x = vExtents.x * adapter.vExtentSigns.x;
	vCoplanarExtent.y = vExtents.y * adapter.vExtentSigns.y;
	vCoplanarExtent.z = vExtents.z * adapter.vExtentSigns.z;

	adapter.shiftedPlane.m_Normal = stayInFrontOfPlane.m_Normal;
	adapter.shiftedPlane.m_Dist = stayInFrontOfPlane.m_Dist + (stayInFrontOfPlane.m_Normal.Dot( vCoplanarExtent ) ); //the dot is known to be negative, shifting the plane back so the extent is coplanar with it.

	return UTIL_FindClosestPassableSpace( vCenter, vExtents, vIndecisivePush, iIterations, vCenterOut, FL_AXIS_DIRECTION_NONE, &adapter );
}


bool FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask ) //assumes the object is already in a mostly passable space
{
	if ( sv_use_find_closest_passable_space.GetBool() == false )
		return true;

	// Don't ever do this to entities with a move parent
	if ( pEntity->GetMoveParent() )
		return true;

#ifndef CLIENT_DLL
	ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "RUNNING FIND CLOSEST PASSABLE SPACE on %s..\n", pEntity->GetDebugName() ) );
#endif

	Vector ptExtents[8]; //ordering is going to be like 3 bits, where 0 is a min on the related axis, and 1 is a max on the same axis, axis order x y z

	float fExtentsValidation[8]; //some points are more valid than others, and this is our measure


	Vector vEntityMaxs;// = pEntity->WorldAlignMaxs();
	Vector vEntityMins;// = pEntity->WorldAlignMins();
	CCollisionProperty *pEntityCollision = pEntity->CollisionProp();
	pEntityCollision->WorldSpaceAABB( &vEntityMins, &vEntityMaxs );



	Vector ptEntityCenter = ((vEntityMins + vEntityMaxs) / 2.0f);
	vEntityMins -= ptEntityCenter;
	vEntityMaxs -= ptEntityCenter;

	Vector ptEntityOriginalCenter = ptEntityCenter;
	
	ptEntityCenter.z += 0.001f; //to satisfy m_IsSwept on first pass

	int iEntityCollisionGroup = pEntity->GetCollisionGroup();

	trace_t traces[2];
	Ray_t entRay;
	//entRay.Init( ptEntityCenter, ptEntityCenter, vEntityMins, vEntityMaxs );
	entRay.m_Extents = vEntityMaxs;
	entRay.m_IsRay = false;
	entRay.m_IsSwept = true;
	entRay.m_StartOffset = vec3_origin;

	Vector vOriginalExtents = vEntityMaxs;	

	Vector vGrowSize = vEntityMaxs / 101.0f;
	vEntityMaxs -= vGrowSize;
	vEntityMins += vGrowSize;
	
	
	Ray_t testRay;
	testRay.m_Extents = vGrowSize;
	testRay.m_IsRay = false;
	testRay.m_IsSwept = true;
	testRay.m_StartOffset = vec3_origin;



	unsigned int iFailCount;
	for( iFailCount = 0; iFailCount != 100; ++iFailCount )
	{
		entRay.m_Start = ptEntityCenter;
		entRay.m_Delta = ptEntityOriginalCenter - ptEntityCenter;

		UTIL_TraceRay( entRay, fMask, pEntity, iEntityCollisionGroup, &traces[0] );
		if( traces[0].startsolid == false )
		{
			Vector vNewPos = traces[0].endpos + (pEntity->GetAbsOrigin() - ptEntityOriginalCenter);
#ifdef CLIENT_DLL
			pEntity->SetAbsOrigin( vNewPos );
#else
			pEntity->Teleport( &vNewPos, NULL, NULL );
#endif
			return true; //current placement worked
		}

		bool bExtentInvalid[8];
		for( int i = 0; i != 8; ++i )
		{
			fExtentsValidation[i] = 0.0f;
			ptExtents[i] = ptEntityCenter;
			ptExtents[i].x += ((i & (1<<0)) ? vEntityMaxs.x : vEntityMins.x);
			ptExtents[i].y += ((i & (1<<1)) ? vEntityMaxs.y : vEntityMins.y);
			ptExtents[i].z += ((i & (1<<2)) ? vEntityMaxs.z : vEntityMins.z);

			bExtentInvalid[i] = enginetrace->PointOutsideWorld( ptExtents[i] );
		}

		unsigned int counter, counter2;
		for( counter = 0; counter != 7; ++counter )
		{
			for( counter2 = counter + 1; counter2 != 8; ++counter2 )
			{

				testRay.m_Delta = ptExtents[counter2] - ptExtents[counter];
				
				if( bExtentInvalid[counter] )
					traces[0].startsolid = true;
				else
				{
					testRay.m_Start = ptExtents[counter];
					UTIL_TraceRay( testRay, fMask, pEntity, iEntityCollisionGroup, &traces[0] );
				}

				if( bExtentInvalid[counter2] )
					traces[1].startsolid = true;
				else
				{
					testRay.m_Start = ptExtents[counter2];
					testRay.m_Delta = -testRay.m_Delta;
					UTIL_TraceRay( testRay, fMask, pEntity, iEntityCollisionGroup, &traces[1] );
				}

				float fDistance = testRay.m_Delta.Length();

				for( int i = 0; i != 2; ++i )
				{
					int iExtent = (i==0)?(counter):(counter2);

					if( traces[i].startsolid )
					{
						fExtentsValidation[iExtent] -= 100.0f;
					}
					else
					{
						fExtentsValidation[iExtent] += traces[i].fraction * fDistance;
					}
				}
			}
		}

		Vector vNewOriginDirection( 0.0f, 0.0f, 0.0f );
		float fTotalValidation = 0.0f;
		for( counter = 0; counter != 8; ++counter )
		{
			if( fExtentsValidation[counter] > 0.0f )
			{
				vNewOriginDirection += (ptExtents[counter] - ptEntityCenter) * fExtentsValidation[counter];
				fTotalValidation += fExtentsValidation[counter];
			}
		}

		if( fTotalValidation != 0.0f )
		{
			ptEntityCenter += (vNewOriginDirection / fTotalValidation);

			//increase sizing
			testRay.m_Extents += vGrowSize;
			vEntityMaxs -= vGrowSize;
			vEntityMins = -vEntityMaxs;
		}
		else
		{
			//no point was valid, apply the indecisive vector
			ptEntityCenter += vIndecisivePush;

			//reset sizing
			testRay.m_Extents = vGrowSize;
			vEntityMaxs = vOriginalExtents;
			vEntityMins = -vEntityMaxs;
		}		
	}

	// X360TBD: Hits in portal devtest
	AssertMsg( IsGameConsole() || iFailCount != 100, "FindClosestPassableSpace() failure." );
	return false;
}

CPortal_Base2D *UTIL_PointIsOnPortalQuad( const Vector vPoint, float fOnPlaneEpsilon, CPortal_Base2D * const *pPortalsToCheck, int iArraySize )
{
	if( pPortalsToCheck == NULL )
		return NULL;

	if( iArraySize == 0 )
		return NULL;

	CPortal_Base2D **pPlanarCandidates = (CPortal_Base2D **)stackalloc( sizeof( CPortal_Base2D * ) * iArraySize );
	int iPlanarCandidateCount = 0;
	for( int i = 0; i != iArraySize; ++i )
	{
		if( fabs( pPortalsToCheck[i]->m_plane_Origin.normal.Dot( vPoint ) - pPortalsToCheck[i]->m_plane_Origin.dist ) < fOnPlaneEpsilon )
		{
			pPlanarCandidates[iPlanarCandidateCount] = pPortalsToCheck[i];
			++iPlanarCandidateCount;
		}
	}

	if( iPlanarCandidateCount == 0 )
		return NULL;

	for( int i = 0; i != iPlanarCandidateCount; ++i )
	{
		Vector vDiff = vPoint - pPlanarCandidates[i]->m_ptOrigin;
		
		if( (fabs( vDiff.Dot( pPlanarCandidates[i]->m_vRight ) ) < pPlanarCandidates[i]->GetHalfWidth()) &&
			(fabs( vDiff.Dot( pPlanarCandidates[i]->m_vUp ) ) < pPlanarCandidates[i]->GetHalfHeight()) )
		{
			return pPlanarCandidates[i];
		}
	}

	return NULL;
}

bool UTIL_Portal_EntityIsInPortalHole( const CPortal_Base2D *pPortal, const CBaseEntity *pEntity )
{
	const CCollisionProperty *pCollisionProp = pEntity->CollisionProp();
	Vector vMins = pCollisionProp->OBBMins();
	Vector vMaxs = pCollisionProp->OBBMaxs();
	Vector vForward, vUp, vRight;
	AngleVectors( pCollisionProp->GetCollisionAngles(), &vForward, &vRight, &vUp );
	Vector ptOrigin = pEntity->GetAbsOrigin();

	Vector ptOBBCenter = pEntity->GetAbsOrigin() + (vMins + vMaxs * 0.5f);
	Vector vExtents = (vMaxs - vMins) * 0.5f;

	vForward *= vExtents.x;
	vRight *= vExtents.y;
	vUp *= vExtents.z;

	return OBBHasFullyContainedIntersectionWithQuad( vForward, vRight, vUp, ptOBBCenter, 
		pPortal->m_vForward, pPortal->m_vForward.Dot( pPortal->m_ptOrigin ), pPortal->m_ptOrigin, 
		pPortal->m_vRight, pPortal->GetHalfWidth() + 1.0f, pPortal->m_vUp, pPortal->GetHalfHeight() + 1.0f );
}

const Vector UTIL_ProjectPointOntoPlane( const Vector& point, const cplane_t& plane )
{
	return point - (DotProduct( point, plane.normal ) - plane.dist) * plane.normal;
}


bool UTIL_PointIsNearPortal( const Vector& point, const CPortal_Base2D* pPortal2D, float planeDist, float radiusReduction )
{
	AssertMsg( pPortal2D != NULL, "Null pointers are bad, and you should feel bad." );

	const cplane_t& portalPlane = pPortal2D->m_plane_Origin;
	Vector transformedPt, origin;
	pPortal2D->WorldToEntitySpace( point, &transformedPt );
	pPortal2D->WorldToEntitySpace( UTIL_ProjectPointOntoPlane( pPortal2D->WorldSpaceCenter(), portalPlane ), &origin );

	AssertMsg( pPortal2D->GetHalfWidth() > radiusReduction && pPortal2D->GetHalfHeight() > radiusReduction, "Reduction of the box is too high." );
	const float halfWidth = pPortal2D->GetHalfWidth() - radiusReduction;
	const float halfHeight = pPortal2D->GetHalfHeight() - radiusReduction;
	const Vector boxMin( origin[0] - planeDist, origin[1] - halfWidth, origin[2] - halfHeight );
	const Vector boxMax( origin[0] + planeDist, origin[1] + halfWidth, origin[2] + halfHeight );

	return (transformedPt[0] >= boxMin[0] && transformedPt[0] <= boxMax[0]) &&
		   (transformedPt[1] >= boxMin[1] && transformedPt[1] <= boxMax[1]) &&
		   (transformedPt[2] >= boxMin[2] && transformedPt[2] <= boxMax[2]);
}


bool UTIL_IsEntityMovingOrRotating( CBaseEntity* pEntity )
{
	Vector vLinearVelocity;
	AngularImpulse vAngularVelocity;

	IPhysicsObject *pEntityPhysicsObject = pEntity->VPhysicsGetObject();
#ifdef GAME_DLL
	if ( pEntityPhysicsObject )
	{
		pEntityPhysicsObject->GetVelocity( &vLinearVelocity, &vAngularVelocity );
	}
	else
	{
		pEntity->GetVelocity( &vLinearVelocity, &vAngularVelocity );
	}
#else
	vLinearVelocity = pEntity->GetAbsVelocity();
	// vAngularVelocity = vec3_origin; //TODO: Find client equivalent of server code above for angular impulse
	QAngleToAngularImpulse( pEntity->GetLocalAngularVelocity(), vAngularVelocity );
#endif

	if ( pEntity->GetAbsVelocity() != vec3_origin )
		return true;

	//ugh, func_brush attached to an animating entity doesn't give the entity a velocity. Check implicit velocity
	if( pEntityPhysicsObject && pEntityPhysicsObject->IsMoveable() )
	{
		Vector vOtherImplicitVelocity;
		pEntityPhysicsObject->GetImplicitVelocity( &vOtherImplicitVelocity, NULL );
		if( vOtherImplicitVelocity.LengthSqr() > 1e-1 )
			return true;
	}

#ifdef GAME_DLL
	CBaseEntity *pCheck = pEntity;
	while( pCheck )
	{
		if( pCheck->GetLocalAngularVelocity() != vec3_angle )
			return true;

		pCheck = pCheck->GetParent();
	}
#endif

	// don't check for the speed, and check for position offset from when the portal is placed instead.
	/*if ( vLinearVelocity.LengthSqr() > 1e-1 || vAngularVelocity.LengthSqr() > 1e-1 )
		return true;*/

	return false;
}


#ifdef CLIENT_DLL
void UTIL_TransformInterpolatedAngle( CInterpolatedVar< QAngle > &qInterped, matrix3x4_t matTransform, float fUpToTime )
{
	int iHead = qInterped.GetHead();
	if( !qInterped.IsValidIndex( iHead ) )
		return;

#ifdef _DEBUG
	float fHeadTime;
	qInterped.GetHistoryValue( iHead, fHeadTime );
#endif

	float fTime;
	QAngle *pCurrent;
	int iCurrent;

	//if( bSkipNewest )
	//	iCurrent = qInterped.GetNext( iHead );
	//else
		iCurrent = iHead;

	while( (pCurrent = qInterped.GetHistoryValue( iCurrent, fTime )) != NULL )
	{
		Assert( (fTime <= fHeadTime) || (iCurrent == iHead) ); //asserting that head is always newest
		
		if( fTime < fUpToTime )
			*pCurrent = TransformAnglesToWorldSpace( *pCurrent, matTransform );

		iCurrent = qInterped.GetNext( iCurrent );
		if( iCurrent == iHead )
			break;
	}

	//qInterped.Interpolate( gpGlobals->curtime );
}

void UTIL_TransformInterpolatedPosition( CInterpolatedVar< Vector > &vInterped, const VMatrix& matTransform, float fUpToTime )
{
	int iHead = vInterped.GetHead();
	if( !vInterped.IsValidIndex( iHead ) )
		return;

#ifdef _DEBUG
	float fHeadTime;
	vInterped.GetHistoryValue( iHead, fHeadTime );
#endif

	float fTime;
	Vector *pCurrent;
	int iCurrent;

	//if( bSkipNewest )
	//	iCurrent = vInterped.GetNext( iHead );
	//else
		iCurrent = iHead;

	while( (pCurrent = vInterped.GetHistoryValue( iCurrent, fTime )) != NULL )
	{
		Assert( (fTime <= fHeadTime) || (iCurrent == iHead) );

		if( fTime < fUpToTime )
		{
			*pCurrent = matTransform * (*pCurrent);


		}

		iCurrent = vInterped.GetNext( iCurrent );
		if( iCurrent == iHead )
			break;
	}

	//vInterped.Interpolate( gpGlobals->curtime );
}
#endif


#ifndef CLIENT_DLL

void CC_Debug_FixMyPosition( void )
{
	CBaseEntity *pPlayer = UTIL_GetCommandClient();

	FindClosestPassableSpace( pPlayer, vec3_origin );
}

static ConCommand debug_fixmyposition("debug_fixmyposition", CC_Debug_FixMyPosition, "Runs FindsClosestPassableSpace() on player.", FCVAR_CHEAT );
#endif


bool IsIn180( float f )
{
	return f >= 0.f && f <= 180.f;
}

bool IsInNeg180( float f )
{
	return f < 0.f && f >= -180.f;
}


float DistTo180( float f )
{
	if ( IsIn180( f ) )
	{
		return 180.f - f;
	}
	else if ( IsInNeg180( f ) )
	{
		return -180 - f;
	}
	else
	{
		Assert("We are out of bound [-180,180]");
		return 0.f;
	}
}


void UTIL_NormalizedAngleDiff( const QAngle& start, const QAngle& end, QAngle* result )
{
	if ( result )
	{
		QAngle angles;
		for ( int i=0; i<3; ++i )
		{
			float a = start[ i ];
			float b = end[ i ];

			float distAto180 = DistTo180( a );
			float distBto180 = DistTo180( b );

			bool bUse180 = ( fabs( distAto180 ) + fabs( distBto180 ) ) < ( fabs( a ) + fabs( b ) );
			bool bDiffSign = Sign( a ) * Sign( b ) < 0;

			if ( bDiffSign )
			{
				if ( bUse180 )
				{
					angles[ i ] = distBto180 - distAto180;
				}
				else
				{
					angles[ i ] = a - b;
				}
			}
			else
			{
				angles[ i ] = a - b;
			}
		}
		
		*result = -angles;
	}
}


//it turns out that using MatrixInverseTR() is theoretically correct. But we need to ensure that these matrices match exactly on the client/server. 
//And computing inverses screws that up just enough (differences of ~0.00005 in the translation some times) to matter. So we compute each from scratch every time
#if defined( CLIENT_DLL )
void UTIL_Portal_ComputeMatrix_ForReal( CPortalRenderable_FlatBasic *pLocalPortal, CPortalRenderable_FlatBasic *pRemotePortal )
#else
void UTIL_Portal_ComputeMatrix_ForReal( CPortal_Base2D *pLocalPortal, CPortal_Base2D *pRemotePortal )
#endif
{
	VMatrix worldToLocal_Rotated;
	worldToLocal_Rotated.m[0][0] = -pLocalPortal->m_vForward.x;
	worldToLocal_Rotated.m[0][1] = -pLocalPortal->m_vForward.y;
	worldToLocal_Rotated.m[0][2] = -pLocalPortal->m_vForward.z;
	worldToLocal_Rotated.m[0][3] = ((Vector)pLocalPortal->m_ptOrigin).Dot( pLocalPortal->m_vForward );

	worldToLocal_Rotated.m[1][0] = pLocalPortal->m_vRight.x;
	worldToLocal_Rotated.m[1][1] = pLocalPortal->m_vRight.y;
	worldToLocal_Rotated.m[1][2] = pLocalPortal->m_vRight.z;
	worldToLocal_Rotated.m[1][3] = -((Vector)pLocalPortal->m_ptOrigin).Dot( pLocalPortal->m_vRight );

	worldToLocal_Rotated.m[2][0] = pLocalPortal->m_vUp.x;
	worldToLocal_Rotated.m[2][1] = pLocalPortal->m_vUp.y;
	worldToLocal_Rotated.m[2][2] = pLocalPortal->m_vUp.z;
	worldToLocal_Rotated.m[2][3] = -((Vector)pLocalPortal->m_ptOrigin).Dot( pLocalPortal->m_vUp );		

	worldToLocal_Rotated.m[3][0] = 0.0f;
	worldToLocal_Rotated.m[3][1] = 0.0f;
	worldToLocal_Rotated.m[3][2] = 0.0f;
	worldToLocal_Rotated.m[3][3] = 1.0f;

	VMatrix remoteToWorld( pRemotePortal->m_vForward, -pRemotePortal->m_vRight, pRemotePortal->m_vUp );
	remoteToWorld.SetTranslation( pRemotePortal->m_ptOrigin );

	//final
	pLocalPortal->m_matrixThisToLinked = remoteToWorld * worldToLocal_Rotated;
}

//MUST be a shared function to prevent floating point precision weirdness in the 100,000th decimal place between client/server that we're attributing to differing register usage.
#if defined( CLIENT_DLL )
void UTIL_Portal_ComputeMatrix( CPortalRenderable_FlatBasic *pLocalPortal, CPortalRenderable_FlatBasic *pRemotePortal )
#else
void UTIL_Portal_ComputeMatrix( CPortal_Base2D *pLocalPortal, CPortal_Base2D *pRemotePortal )
#endif
{
	if ( pRemotePortal != NULL )
	{
		UTIL_Portal_ComputeMatrix_ForReal( pLocalPortal, pRemotePortal );
		UTIL_Portal_ComputeMatrix_ForReal( pRemotePortal, pLocalPortal );
	}
	else
	{
		pLocalPortal->m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	}
}


CEG_NOINLINE bool UTIL_IsPaintableSurface( const csurface_t& surface )
{
	CEG_GCV_PRE();
	static const unsigned short CEG_SURF_NO_PAINT_FLAG = CEG_GET_CONSTANT_VALUE( SurfNoPaintFlag );
	CEG_GCV_POST();
	return !( surface.flags & CEG_SURF_NO_PAINT_FLAG );
}



float UTIL_PaintBrushEntity( CBaseEntity* pBrushEntity, const Vector& contactPoint, PaintPowerType power, float flPaintRadius, float flAlphaPercent )
{
	if ( !pBrushEntity )
	{
		// HACK HACK: Fix it for real Bank! :)
		return 0.0f;
	}

	if ( !pBrushEntity->IsBSPModel() )
	{
		return 0.0f;
	}

	Vector vEntitySpaceContactPoint;
	pBrushEntity->WorldToEntitySpace( contactPoint, &vEntitySpaceContactPoint );

	if ( !engine->SpherePaintSurface( pBrushEntity->GetModel(), vEntitySpaceContactPoint, power, flPaintRadius, flAlphaPercent ) )
		return 0.0f;
	return flPaintRadius;
}


PaintPowerType UTIL_Paint_TracePower( CBaseEntity* pBrushEntity, const Vector& contactPoint, const Vector& vContactNormal )
{
	if ( !pBrushEntity->IsBSPModel() )
	{
		return NO_POWER;
	}

	CUtlVector<BYTE> color;

	// Transform contact point from world to entity space
	Vector vEntitySpaceContactPoint;
	pBrushEntity->WorldToEntitySpace( contactPoint, &vEntitySpaceContactPoint );

	// transform contact normal
	Vector vTransformedContactNormal;
	VectorRotate( vContactNormal, -pBrushEntity->GetAbsAngles(), vTransformedContactNormal );

	engine->SphereTracePaintSurface( pBrushEntity->GetModel(), vEntitySpaceContactPoint, vTransformedContactNormal, sv_paint_detection_sphere_radius.GetFloat(), color );

	return MapColorToPower( color );
}


bool UTIL_Paint_Reflect( const trace_t& tr, Vector& vStart, Vector& vDir, PaintPowerType reflectPower /* = REFLECT_POWER */ )
{
	// check for reflect paint
	if ( engine->HasPaintmap() && tr.m_pEnt && tr.m_pEnt->IsBSPModel() )
	{
		PaintPowerType power = UTIL_Paint_TracePower( tr.m_pEnt, tr.endpos, tr.plane.normal );
		if ( power == reflectPower )
		{
			Vector vecIn = tr.endpos - tr.startpos;
			if ( DotProduct( vecIn.Normalized(), tr.plane.normal ) > -0.99f )
			{
				Vector vecReflect = vecIn - 2 * DotProduct( vecIn, tr.plane.normal ) * tr.plane.normal;

				vStart = tr.endpos;
				vDir = vecReflect.Normalized();

				// FIXME: Bring this back for DLC2
				//if ( reflect_paint_vertical_snap.GetBool() )
				{
					float flDot = DotProduct( tr.plane.normal, Vector(0.f,0.f,1.f) );

					if ( vDir.z > 0.1 && flDot > 0 && flDot < 1  )
					{
						vDir = Vector( 0.f, 0.f, 1.f );
					}
					else if( vDir.z < -0.1f && flDot > 0 && flDot < 1 )
					{
						vDir = Vector( 0.f, 0.f, -1.f );
					}
				}

				return true;
			}
		}
	}

	return false;
}



//Extends a radius through portals. For various radius effects like explosions and pushing
void ExtendRadiusThroughPortals( const Vector &vecOrigin, const QAngle &vecAngles, float flRadius, PortalRadiusExtensionVector &portalRadiusExtensions )
{
	//Always add the original point
	PortalRadiusExtension_t extension;
	extension.pPortalFrom = NULL;
	extension.pPortalTo = NULL;
	extension.vecOrigin = vecOrigin;
	extension.vecAngles = vecAngles;
	portalRadiusExtensions.AddToTail( extension );

	int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CPortal_Base2D **pAllPortals = CPortal_Base2D_Shared::AllPortals.Base();
		CUtlVector<CPortal_Base2D*> testedPortals; //The portals that have already been tested
		for( int i = 0; i != iPortalCount; ++i )
		{
			CPortal_Base2D *pPortal = pAllPortals[i];

			//If this portal exists and is linked
			if( !pPortal || !pPortal->IsActivedAndLinked() )
			{
				continue;
			}

			//Check if this portal has already been tested
			int iTestedPortalsCount = testedPortals.Count();
			bool bTestThroughPortal = true;
			for( int j = 0; j < iTestedPortalsCount; ++j )
			{
				//If either this portal or it's linked partner has been tested, then don't test it again
				if( testedPortals[j] == pPortal || testedPortals[j] == pPortal->GetLinkedPortal() )
				{
					bTestThroughPortal = false;
					break;
				}
			}

			//If we should test through this portal
			if( bTestThroughPortal )
			{
				//If this portal is near our radius
				float flDistSqr = pPortal->GetAbsOrigin().DistToSqr( vecOrigin );
				if( flDistSqr <= flRadius * flRadius )
				{
					//Check which portal is facing the origin.
					Vector vecOriginForward;
					AngleVectors( vecAngles, &vecOriginForward );

					Vector vecPortal1ToOrigin = pPortal->GetAbsOrigin() - vecOrigin;
					VectorNormalize( vecPortal1ToOrigin );

					Vector vecPortal2ToOrigin = pPortal->m_hLinkedPortal.Get()->GetAbsOrigin() - vecOrigin;
					VectorNormalize( vecPortal2ToOrigin );

					float flDot1 = 1.0f - abs( DotProduct( vecPortal1ToOrigin, vecOriginForward ) );
					float flDot2 = 1.0f - abs( DotProduct( vecPortal2ToOrigin, vecOriginForward ) );

					//Use the portal that is facing the origin for the transformations
					if( flDot2 < flDot1 )
					{
						pPortal = pPortal->m_hLinkedPortal.Get();
					}

					VMatrix matThisToLinked = pPortal->MatrixThisToLinked();

					//Transform the center to the other side of the portal
					Vector vecTransformedOrigin;
					UTIL_Portal_PointTransform( matThisToLinked, vecOrigin, vecTransformedOrigin );

					//Transform the angles to the other side of the portal
					QAngle vecTransformedAngles;
					UTIL_Portal_AngleTransform( matThisToLinked, vecAngles, vecTransformedAngles );

					//Extend the radius through this portal
					PortalRadiusExtension_t extension;
					extension.pPortalFrom = pPortal;
					extension.pPortalTo = pPortal->m_hLinkedPortal.Get();
					extension.vecOrigin = vecTransformedOrigin;
					extension.vecAngles = vecTransformedAngles;
					portalRadiusExtensions.AddToTail( extension );

					//Add this portal to the list of tested portals
					testedPortals.AddToTail( pPortal );
				}
			}

		}// For all the portals in the level

	} //If there are portals in the level
}






void UTIL_Portal_TraceRay_PreTraceChanges( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall )
{
	Assert( pPortal->m_PortalSimulator.IsReadyToSimulate() ); //a trace shouldn't make it down this far if the portal is incapable of changing the results of the trace

	CTraceFilterHitAll traceFilterHitAll;
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilterHitAll;
	}

	pTrace->fraction = 2.0f;
	pTrace->startsolid = true;
	pTrace->allsolid = true;

	trace_t TempTrace;
	int counter;

	const CPortalSimulator &portalSimulator = pPortal->m_PortalSimulator;
	CPortalSimulator *pLinkedPortalSimulator = portalSimulator.GetLinkedPortalSimulator();

	//bool bTraceDisplacements = sv_portal_trace_vs_displacements.GetBool();
	bool bTraceStaticProps = sv_portal_trace_vs_staticprops.GetBool();
	if( sv_portal_trace_vs_holywall.GetBool() == false )
		bTraceHolyWall = false;

	bool bTraceTransformedGeometry = ( (pLinkedPortalSimulator != NULL) && bTraceHolyWall && (portalSimulator.IsRayInPortalHole( ray ) != RIPHR_NOT_TOUCHING_HOLE) );	

	bool bCopyBackBrushTraceData = false;



	// Traces vs world
	if( pTraceFilter->GetTraceType() != TRACE_ENTITIES_ONLY )
	{
		//trace_t RealTrace;
		//enginetrace->TraceRay( ray, fMask, pTraceFilter, &RealTrace );
		if( sv_portal_trace_vs_world.GetBool() )
		{
			for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( portalSimulator.GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
			{
				if( ((portalSimulator.GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].iSolidMask & fMask) != 0) &&
					portalSimulator.GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
				{
					physcollision->TraceBoxAA( ray, portalSimulator.GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, &TempTrace );
					if( (TempTrace.startsolid == false) && (TempTrace.fraction < pTrace->fraction) ) //never allow something to be stuck in the tube, it's more of a last-resort guide than a real collideable
					{
						*pTrace = TempTrace;
						bCopyBackBrushTraceData = true;
					}
				}
			}
		}

		if( portalSimulator.GetInternalData().Simulation.Static.World.Displacements.pCollideable && sv_portal_trace_vs_world.GetBool() && portal_clone_displacements.GetBool() )
		{
			physcollision->TraceBoxAA( ray, portalSimulator.GetInternalData().Simulation.Static.World.Displacements.pCollideable, &TempTrace );
			if( (TempTrace.startsolid == false) && (TempTrace.fraction < pTrace->fraction) ) //never allow something to be stuck in the tube, it's more of a last-resort guide than a real collideable
			{
				*pTrace = TempTrace;
				bCopyBackBrushTraceData = true;
			}
		}

		if( bTraceHolyWall )
		{
			if( portalSimulator.GetInternalData().Simulation.Static.Wall.Local.Tube.pCollideable )
			{
				physcollision->TraceBoxAA( ray, portalSimulator.GetInternalData().Simulation.Static.Wall.Local.Tube.pCollideable, &TempTrace );

				if( (TempTrace.startsolid == false) && (TempTrace.fraction < pTrace->fraction) ) //never allow something to be stuck in the tube, it's more of a last-resort guide than a real collideable
				{
					*pTrace = TempTrace;
					bCopyBackBrushTraceData = true;
				}
			}

			for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( portalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
			{
				if( ((portalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].iSolidMask & fMask) != 0) &&
					portalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable )
				{
					physcollision->TraceBoxAA( ray, portalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable, &TempTrace );
					if( (TempTrace.fraction < pTrace->fraction) )
					{
						*pTrace = TempTrace;
						bCopyBackBrushTraceData = true;
					}
				}
			}

			//if( portalSimulator.GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable && sv_portal_trace_vs_world.GetBool() )
			if( bTraceTransformedGeometry )
			{
				for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
				{
					if( ((pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].iSolidMask & fMask) != 0) &&
						pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
					{
						physcollision->TraceBox( ray, pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, portalSimulator.GetInternalData().Placement.ptaap_LinkedToThis.ptOriginTransform, portalSimulator.GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform, &TempTrace );
						if( (TempTrace.fraction < pTrace->fraction) )
						{
							*pTrace = TempTrace;
							bCopyBackBrushTraceData = true;
						}
					}
				}
			}
		}	

		if( bCopyBackBrushTraceData )
		{
			pTrace->surface = portalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.surface;
			pTrace->contents = portalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.contents;
			pTrace->m_pEnt = portalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.pEntity;

			bCopyBackBrushTraceData = false;
		}
	}

	// Traces vs entities
	if( pTraceFilter->GetTraceType() != TRACE_WORLD_ONLY )
	{
		bool bFilterStaticProps = (pTraceFilter->GetTraceType() == TRACE_EVERYTHING_FILTER_PROPS);

		//solid entities
		CPortalCollideableEnumerator enumerator( pPortal );

		int PartitionMask;
#if defined( CLIENT_DLL )
		PartitionMask = PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_STATIC_PROPS;
#else
		PartitionMask = PARTITION_ENGINE_SOLID_EDICTS | PARTITION_ENGINE_STATIC_PROPS;
#endif

		::partition->EnumerateElementsAlongRay( PartitionMask, ray, false, &enumerator );
		for( counter = 0; counter != enumerator.m_iHandleCount; ++counter )
		{
			if( staticpropmgr->IsStaticProp( enumerator.m_pHandles[counter] ) )
			{
				//if( bFilterStaticProps && !pTraceFilter->ShouldHitEntity( enumerator.m_pHandles[counter], fMask ) )
				continue; //static props are handled separately, with clipped versions
			}
			else if ( !pTraceFilter->ShouldHitEntity( enumerator.m_pHandles[counter], fMask ) )
			{
				continue;
			}

			CBaseEntity *pEnumeratedEntity = EntityFromEntityHandle( enumerator.m_pHandles[counter] );;

			//If we have a carved representation of this entity, trace against that instead of the real thing
			CPhysCollide *pCarvedCollide = portalSimulator.IsEntityCarvedByPortal( pEnumeratedEntity ) ? portalSimulator.GetCollideForCarvedEntity( pEnumeratedEntity ) : NULL;
			if( pCarvedCollide != NULL )
			{
				ICollideable *pUncarvedCollideable = pEnumeratedEntity->GetCollideable();
				physcollision->TraceBox( ray, pCarvedCollide, pUncarvedCollideable->GetCollisionOrigin(), pUncarvedCollideable->GetCollisionAngles(), &TempTrace );

				if( (TempTrace.fraction < pTrace->fraction)  )
				{
					//copy the trace data from the carved trace
					*pTrace = TempTrace;

					//then trace against the real thing for surface data.
					//TODO: There's got to be a way to store this off and look it up intelligently. But I can't seem to find surface info without a trace, making the results only valid for that trace.
					enginetrace->ClipRayToEntity( ray, fMask, enumerator.m_pHandles[counter], &TempTrace );
					pTrace->contents = TempTrace.contents;
					pTrace->surface = TempTrace.surface;
					pTrace->m_pEnt = TempTrace.m_pEnt;
				}
			}
			else
			{
				enginetrace->ClipRayToEntity( ray, fMask, enumerator.m_pHandles[counter], &TempTrace );
				if( (TempTrace.fraction < pTrace->fraction)  )
					*pTrace = TempTrace;
			}			
		}




		if( bTraceStaticProps )
		{
			//local clipped static props
			{
				int iLocalStaticCount = portalSimulator.GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Count();
				if( iLocalStaticCount != 0 && portalSimulator.GetInternalData().Simulation.Static.World.StaticProps.bCollisionExists )
				{
					const PS_SD_Static_World_StaticProps_ClippedProp_t *pCurrentProp = portalSimulator.GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Base();
					const PS_SD_Static_World_StaticProps_ClippedProp_t *pStop = pCurrentProp + iLocalStaticCount;
					Vector vTransform = vec3_origin;
					QAngle qTransform = vec3_angle;

					do
					{
						if( (!bFilterStaticProps) || pTraceFilter->ShouldHitEntity( pCurrentProp->pSourceProp, fMask ) )
						{
							physcollision->TraceBox( ray, pCurrentProp->pCollide, vTransform, qTransform, &TempTrace );
							if( (TempTrace.fraction < pTrace->fraction) )
							{
								*pTrace = TempTrace;
								pTrace->surface.flags = pCurrentProp->iTraceSurfaceFlags;
								pTrace->surface.surfaceProps = pCurrentProp->iTraceSurfaceProps;
								pTrace->surface.name = pCurrentProp->szTraceSurfaceName;
								pTrace->contents = pCurrentProp->iTraceContents;
								pTrace->m_pEnt = pCurrentProp->pTraceEntity;
							}
						}

						++pCurrentProp;
					}
					while( pCurrentProp != pStop );
				}
			}

			if( bTraceHolyWall )
			{
				//remote clipped static props transformed into our wall space
				if( bTraceTransformedGeometry && (pTraceFilter->GetTraceType() != TRACE_WORLD_ONLY) && sv_portal_trace_vs_staticprops.GetBool() )
				{
					int iLocalStaticCount = pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Count();
					if( iLocalStaticCount != 0 )
					{
						const PS_SD_Static_World_StaticProps_ClippedProp_t *pCurrentProp = pLinkedPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Base();
						const PS_SD_Static_World_StaticProps_ClippedProp_t *pStop = pCurrentProp + iLocalStaticCount;
						Vector vTransform = portalSimulator.GetInternalData().Placement.ptaap_LinkedToThis.ptOriginTransform;
						QAngle qTransform = portalSimulator.GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform;

						do
						{
							if( (!bFilterStaticProps) || pTraceFilter->ShouldHitEntity( pCurrentProp->pSourceProp, fMask ) )
							{
								physcollision->TraceBox( ray, pCurrentProp->pCollide, vTransform, qTransform, &TempTrace );
								if( (TempTrace.fraction < pTrace->fraction) )
								{
									*pTrace = TempTrace;
									pTrace->surface.flags = pCurrentProp->iTraceSurfaceFlags;
									pTrace->surface.surfaceProps = pCurrentProp->iTraceSurfaceProps;
									pTrace->surface.name = pCurrentProp->szTraceSurfaceName;
									pTrace->contents = pCurrentProp->iTraceContents;
									pTrace->m_pEnt = pCurrentProp->pTraceEntity;
								}
							}

							++pCurrentProp;
						}
						while( pCurrentProp != pStop );
					}
				}
			}
		}
	}

	if( pTrace->fraction > 1.0f ) //this should only happen if there was absolutely nothing to trace against
	{
		//AssertMsg( 0, "Nothing to trace against" );
		memset( pTrace, 0, sizeof( trace_t ) );
		pTrace->fraction = 1.0f;
		pTrace->startpos = ray.m_Start - ray.m_StartOffset;
		pTrace->endpos = pTrace->startpos + ray.m_Delta;
	}
	else if ( pTrace->fraction < 0 )
	{
		// For all brush traces, use the 'portal backbrush' surface surface contents
		// BUGBUG: Doing this is a great solution because brushes near a portal
		// will have their contents and surface properties homogenized to the brush the portal ray hit.
		pTrace->contents = portalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.contents;
		pTrace->surface = portalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.surface;
		pTrace->m_pEnt = portalSimulator.GetInternalData().Simulation.Static.SurfaceProperties.pEntity;
	}
}


void UTIL_Portal_Laser_Prevent_Tilting( Vector& vDirection )
{
	if ( fabs( vDirection.z ) < 0.1 )
	{
		vDirection.z = 0.f;
		vDirection.NormalizeInPlace();
	}
}



void UTIL_DebugOverlay_Polyhedron( const CPolyhedron *pPolyhedron, int red, int green, int blue, bool noDepthTest, float flDuration, const matrix3x4_t *pTransform )
{
	const Vector *pPoints;
	if( pTransform )
	{
		Vector *pPointsTransform;
		pPoints = pPointsTransform = (Vector *)stackalloc( sizeof( Vector ) * pPolyhedron->iVertexCount );

		for( int i = 0; i != pPolyhedron->iVertexCount; ++i )
		{
			VectorTransform( pPolyhedron->pVertices[i], *pTransform, pPointsTransform[i] );
		}
	}
	else
	{
		pPoints = pPolyhedron->pVertices;
	}

	for( int i = 0; i != pPolyhedron->iLineCount; ++i )
	{
		NDebugOverlay::Line( pPoints[pPolyhedron->pLines[i].iPointIndices[0]], pPoints[pPolyhedron->pLines[i].iPointIndices[1]], red, green, blue, noDepthTest, flDuration );
	}
}


void UTIL_DebugOverlay_CPhysCollide( const CPhysCollide *pCollide, int red, int green, int blue, bool noDepthTest, float flDuration, const matrix3x4_t *pTransform )
{
	Vector *outVerts;
	int vertCount = physcollision->CreateDebugMesh( pCollide, &outVerts );
	int triCount = vertCount / 3;
	
	const Vector *pPoints;
	if( pTransform )
	{
		Vector *pPointsTransform;
		pPoints = pPointsTransform = (Vector *)stackalloc( sizeof( Vector ) * vertCount );

		for( int i = 0; i != vertCount; ++i )
		{
			VectorTransform( outVerts[i], *pTransform, pPointsTransform[i] );
		}
	}
	else
	{
		pPoints = outVerts;
	}

	for( int i = 0; i != triCount; ++i )
	{
		int iStartVert = (i * 3);
		NDebugOverlay::Line( pPoints[iStartVert], pPoints[iStartVert + 1], red, green, blue, noDepthTest, flDuration );
		NDebugOverlay::Line( pPoints[iStartVert + 1], pPoints[iStartVert + 2], red, green, blue, noDepthTest, flDuration );
		NDebugOverlay::Line( pPoints[iStartVert + 2], pPoints[iStartVert], red, green, blue, noDepthTest, flDuration );
	}

	physcollision->DestroyDebugMesh( vertCount, outVerts );
}


bool UTIL_IsCollideableIntersectingPhysCollide( ICollideable *pCollideable, const CPhysCollide *pCollide, const Vector &vPhysCollideOrigin, const QAngle &qPhysCollideAngles )
{
	vcollide_t *pVCollide = modelinfo->GetVCollide( pCollideable->GetCollisionModel() );
	trace_t Trace;

	if( pVCollide != NULL )
	{
		Vector ptEntityPosition = pCollideable->GetCollisionOrigin();
		QAngle qEntityAngles = pCollideable->GetCollisionAngles();

		for( int i = 0; i != pVCollide->solidCount; ++i )
		{
			physcollision->TraceCollide( ptEntityPosition, ptEntityPosition, pVCollide->solids[i], qEntityAngles, pCollide, vPhysCollideOrigin, qPhysCollideAngles, &Trace );

			if( Trace.startsolid )
				return true;
		}
	}
	else
	{
		//use AABB
		Vector vMins, vMaxs, ptCenter;
		pCollideable->WorldSpaceSurroundingBounds( &vMins, &vMaxs );
		ptCenter = (vMins + vMaxs) * 0.5f;
		vMins -= ptCenter;
		vMaxs -= ptCenter;
		physcollision->TraceBox( ptCenter, ptCenter, vMins, vMaxs, pCollide, vPhysCollideOrigin, qPhysCollideAngles, &Trace );

		return Trace.startsolid;
	}

	return false;
}

CBasePlayer* UTIL_OtherPlayer( CBasePlayer const* pPlayer )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pOtherPlayer = UTIL_PlayerByIndex( i );
		if ( pOtherPlayer != NULL && pOtherPlayer != pPlayer )
			return pOtherPlayer;
	}

	return NULL;
}

#ifdef GAME_DLL

CBasePlayer* UTIL_OtherConnectedPlayer( CBasePlayer const* pPlayer )
{
	CBasePlayer *pOtherPlayer = UTIL_OtherPlayer( pPlayer );
	if ( pOtherPlayer && pOtherPlayer->IsConnected() )
	{
		return pOtherPlayer;
	}

	return NULL;
}


bool CBrushEntityList::EnumEntity( IHandleEntity *pHandleEntity )
{
	if ( !pHandleEntity )
		return true;

	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
	if ( !pEntity )
		return true;

	if ( pEntity->IsBSPModel() )
	{
		m_BrushEntitiesToPaint.AddToTail( pEntity );
	}

	return true;
}


void UTIL_FindBrushEntitiesInSphere( CBrushEntityList& brushEnum, const Vector& vCenter, float flRadius )
{
	Vector vExtents = flRadius * Vector( 1.f, 1.f, 1.f );
	enginetrace->EnumerateEntities( vCenter - vExtents, vCenter + vExtents, &brushEnum );
}

#endif
