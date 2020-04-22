//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//============================================================================//

#include "cbase.h"
#include <algorithm>
#include <functional>
#include "paint_blobs_shared.h"
#include "debugoverlay_shared.h"
#include "portal_base2d_shared.h"
#include "paint_cleanser_manager.h"
#include "fmtstr.h"
#include "paintable_entity.h"
#include "portal_player_shared.h"
#include "vprof.h"
#include "datacache/imdlcache.h"
#include "raytrace.h"
#include "prop_portal_shared.h"
#include "mathlib/ssequaternion.h"

// define this when we want to prefetch blob data in PaintBlobUpdate()
//#define BLOB_PREFETCH

// define this to debug blob SIMD update
//#define BLOB_SIMD_DEBUG
#ifdef BLOB_SIMD_DEBUG
#define BLOB_IN_BEAM_ERROR 1.f
#endif

#ifdef BLOB_PREFETCH
#include "cache_hints.h"
#endif

#ifdef CLIENT_DLL
#include "c_trigger_paint_cleanser.h"
#include "c_paintblob.h"
#include "c_world.h"

const Color g_BlobDebugColor( 0, 255, 255 );

ConVar debug_paint_client_blobs( "debug_paint_client_blobs", "0" );

#else

#include "trigger_paint_cleanser.h"
#include "cpaintblob.h"
#include "world.h"

ConVar debug_paint_server_blobs( "debug_paint_server_blobs", "0", FCVAR_DEVELOPMENTONLY );
ConVar debug_paintblobs_streaking( "debug_paintblobs_streaking", "0", FCVAR_DEVELOPMENTONLY );

extern ConVar phys_pushscale;

const Color g_BlobDebugColor( 255, 0, 255 );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VPROF_BUDGETGROUP_PAINTBLOB	_T("Paintblob")

ConVar paintblob_collision_box_size("paintblob_collision_box_size", "60.f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY);

ConVar paintblob_gravity_scale( "paintblob_gravity_scale", "1.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The gravity scale of the paint blobs." );
ConVar paintblob_air_drag( "paintblob_air_drag", "0.1f", FCVAR_REPLICATED | FCVAR_CHEAT, "The air drag applied to the paint blobs." );
ConVar paintblob_minimum_portal_exit_velocity( "paintblob_minimum_portal_exit_velocity", "225.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The minimum velocity of the paint blobs on exiting portals." );

// Blobulator radius scale
ConVar paintblob_min_radius_scale("paintblob_min_radius_scale", "0.7f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintblob_max_radius_scale("paintblob_max_radius_scale", "1.0f", FCVAR_REPLICATED | FCVAR_CHEAT );

// streak
ConVar paintblob_radius_while_streaking( "paintblob_radius_while_streaking", "0.3f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintblob_streak_angle_threshold( "paintblob_streak_angle_threshold", "45.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The angle of impact below which the paint blobs will streak paint." );
ConVar paintblob_streak_trace_range( "paintblob_streak_trace_range", "20.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The range of the trace for the paint blobs while streaking." );
ConVar paintblob_streak_particles_enabled("paintblob_streak_particles_enabled", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_REPLICATED );

//Tractor beam
ConVar paintblob_tbeam_accel( "paintblob_tbeam_accel", "200.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The acceleration of the paint blobs while in a tractor beam to get up to tractor beam speed" );
ConVar paintblob_tbeam_vortex_circulation( "paintblob_tbeam_vortex_circulation", "30000.f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintblob_tbeam_portal_vortex_circulation( "paintblob_tbeam_portal_vortex_circulation", "60000.f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintblob_tbeam_vortex_radius_rate( "paintblob_tbeam_vortex_radius_rate", "100.f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintblob_tbeam_vortex_accel( "paintblob_tbeam_vortex_accel", "300.f", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar paintblob_tbeam_vortex_distance( "paintblob_tbeam_vortex_distance", "50.f", FCVAR_REPLICATED | FCVAR_CHEAT , "Blob will do vortex if blob's distance from start or end point of the beam is within this distance");

//Limited range for blobs
ConVar paintblob_limited_range( "paintblob_limited_range", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "If the paintblobs have a limited range." );
ConVar paintblob_lifetime( "paintblob_lifetime", "1.5f", FCVAR_REPLICATED | FCVAR_CHEAT, "The lifetime of the paintblobs if they have a limited range." );

#ifdef _X360
ConVar paintblob_update_per_second( "paintblob_update_per_second", "30.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The number of times the blobs movement code is run per second." );
#else
ConVar paintblob_update_per_second( "paintblob_update_per_second", "60.0f", FCVAR_REPLICATED | FCVAR_CHEAT, "The number of times the blobs movement code is run per second." );
#endif

ConVar debug_beam_badsection( "debug_beam_badsection", "0", FCVAR_REPLICATED | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

const int BLOB_TRACE_STATIC_MASK = CONTENTS_SOLID | CONTENTS_WINDOW | CONTENTS_HITBOX | CONTENTS_DEBRIS | CONTENTS_WATER | CONTENTS_SLIME;
const int BLOB_TRACE_DYNAMIC_MASK = CONTENTS_SOLID | CONTENTS_HITBOX | CONTENTS_MOVEABLE | CONTENTS_MONSTER | CONTENTS_DEBRIS;

const int MAX_BLOB_TRACE_ENTITY_RESULTS = 64;

extern ConVar sv_gravity;
extern ConVar player_can_use_painted_power;
extern ConVar player_paint_effects_enabled;


ConVar paintblob_beam_radius_offset("paintblob_beam_radius_offset", "15.f", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );
float UTil_Blob_BeamRadiusOffset( float flBeamRadius )
{
	float flOutput = flBeamRadius - paintblob_beam_radius_offset.GetFloat();
	Assert( flOutput > 0.f );
	return ( flOutput > 0.f ) ? flOutput : 1.f;
}

float UTil_Blob_TBeamLinearForce( float flLinearForce )
{
	// motion controller of the beam moves other entities half speed of the blobs
	return 0.5f * fabs( flLinearForce );
}

CBasePaintBlob::CBasePaintBlob() : m_flDestVortexRadius( 0.f ),
									m_flCurrentVortexRadius( 0.f ),
									m_flCurrentVortexSpeed( 0.f ),
									m_flVortexDirection(1.f), // -1.f or 1.f

									// positions & velocities
									m_vecTempEndPosition( vec3_origin ),
									m_vecTempEndVelocity( vec3_origin ),
									m_vecPosition( vec3_origin ),
									m_vecPrevPosition( vec3_origin ),
									m_vecVelocity( vec3_origin ),

									// normal for particles effect
									m_vContactNormal( vec3_origin ),

									m_paintType( NO_POWER ),
									m_hOwner( NULL ),
									m_MoveState( PAINT_BLOB_AIR_MOVE ),

									// life time
									m_flLifeTime( 0.f ),
									
									//Streaking
									m_vecStreakDir( vec3_origin ),
									m_bStreakDirChanged( false ),
									m_flStreakTimer( 0.f ),
									m_flStreakSpeedDampenRate( 0.f ),
									
									//Tractor beam
									m_bInTractorBeam( false ),
									
									m_bDeleteFlag( false ),
									
									// update time
									m_flAccumulatedTime( 0.0 ),
									m_flLastUpdateTime( 0.0 ),
									
									m_flRadiusScale( 0.f ),
									
									m_bShouldPlayEffect( false ),
									
									m_bSilent( false ),
									
									// ghost blob!!!
									m_hPortal( NULL ),
									
									// optimize trace
									m_vCollisionBoxCenter( vec3_origin ),
									m_bCollisionBoxHitSolid( false ),

									m_bDrawOnly( false ),
									
									// num teleported
									m_bTeleportedThisFrame( false ),
									m_nTeleportationCount( 0 )
{
}


CBasePaintBlob::~CBasePaintBlob()
{
}


void CBasePaintBlob::Init( const Vector &vecOrigin, const Vector &vecVelocity, int paintType, float flMaxStreakTime, float flStreakSpeedDampenRate, CBaseEntity* pOwner, bool bSilent, bool bDrawOnly )
{
	m_vecPosition = vecOrigin;
	m_vecPrevPosition = vecOrigin;
	SetVelocity( vecVelocity );
	m_paintType = static_cast<PaintPowerType>( paintType );

	//Set up the streaking properties of the blob
	m_flStreakTimer = flMaxStreakTime;
	m_flStreakSpeedDampenRate = flStreakSpeedDampenRate;
	m_vecStreakDir = vec3_origin;

	//Set the default move state for the blob
	m_MoveState = PAINT_BLOB_AIR_MOVE;

	// set values when blobs hit tbeam
	m_bInTractorBeam = false;
	m_flVortexDirection = Sign( RandomFloat(-1.f, 1.f) );
	m_flDestVortexRadius = RandomFloat( 0.1, 1.f );
	m_flCurrentVortexSpeed = 0.f;

	m_flAccumulatedTime = 0.f;
	m_flLastUpdateTime = gpGlobals->curtime;

	//Set up the radius for the blob
	m_flRadiusScale = RandomFloat( paintblob_min_radius_scale.GetFloat(), paintblob_max_radius_scale.GetFloat() );

	m_bShouldPlayEffect = false;

	m_bSilent = bSilent;

	ResetGhostState();

	m_vCollisionBoxCenter = vecOrigin;
	m_bCollisionBoxHitSolid = CheckCollisionBoxAgainstWorldAndStaticProps();

	m_hOwner = pOwner;
	m_bShouldPlaySound = false;

	m_bDrawOnly = bDrawOnly;
}

const Vector& CBasePaintBlob::GetTempEndPosition( void ) const
{
	return m_vecTempEndPosition;
}

void CBasePaintBlob::SetTempEndPosition( const Vector &vecTempEndPosition )
{
	m_vecTempEndPosition = vecTempEndPosition;
}


const Vector& CBasePaintBlob::GetTempEndVelocity( void ) const
{
	return m_vecTempEndVelocity;
}


void CBasePaintBlob::SetTempEndVelocity( const Vector &vecTempEndVelocity )
{
	m_vecTempEndVelocity = vecTempEndVelocity;
}


const Vector& CBasePaintBlob::GetPosition( void ) const
{
	return m_vecPosition;
}


void CBasePaintBlob::SetPosition( const Vector &vecPosition )
{
	m_vecPrevPosition = m_vecPosition;
	m_vecPosition = vecPosition;
}


const Vector& CBasePaintBlob::GetPrevPosition() const
{
	return m_vecPrevPosition;
}


void CBasePaintBlob::SetPrevPosition( const Vector& vPrevPosition )
{
	m_vecPrevPosition = vPrevPosition;
}


const Vector& CBasePaintBlob::GetVelocity( void ) const
{
	return m_vecVelocity;
}


void CBasePaintBlob::SetVelocity( const Vector &vecVelocity )
{
	m_vecVelocity = vecVelocity;
}


const Vector& CBasePaintBlob::GetStreakDir() const
{
	return m_vecStreakDir;
}


PaintPowerType CBasePaintBlob::GetPaintPowerType( void ) const
{
	return m_paintType;
}


PaintBlobMoveState CBasePaintBlob::GetMoveState( void ) const
{
	return m_MoveState;
}


void CBasePaintBlob::SetMoveState( PaintBlobMoveState moveState )
{
	m_MoveState = moveState;
}


float CBasePaintBlob::GetVortexDirection() const
{
	return m_flVortexDirection;
}


bool CBasePaintBlob::ShouldDeleteThis() const
{
	return m_bDeleteFlag;
}


void CBasePaintBlob::SetDeletionFlag( bool bDelete )
{
	m_bDeleteFlag = bDelete;
}


bool CBasePaintBlob::IsStreaking( void ) const
{
	return m_MoveState == PAINT_BLOB_STREAK_MOVE;
}


float CBasePaintBlob::GetLifeTime() const
{
	return m_flLifeTime;
}


void CBasePaintBlob::UpdateLifeTime( float flLifeTime )
{
	m_flLifeTime += flLifeTime;
}


void CBasePaintBlob::SetRadiusScale( float flRadiusScale )
{
	m_flRadiusScale = flRadiusScale;
}


float CBasePaintBlob::GetRadiusScale( void ) const
{
	if( m_MoveState == PAINT_BLOB_STREAK_MOVE )
	{
		return paintblob_radius_while_streaking.GetFloat();
	}

	return m_flRadiusScale;
}


void CBasePaintBlob::GetGhostMatrix( VMatrix& matGhostTransform )
{
	CProp_Portal *pPortal = assert_cast< CProp_Portal* >( m_hPortal.Get() );
	Assert( pPortal );

	if ( pPortal )
	{
		matGhostTransform = pPortal->MatrixThisToLinked();
	}
}


CTrigger_TractorBeam* CBasePaintBlob::GetCurrentBeam() const
{
	if ( m_beamHistory.m_beams.Count() == 0 )
	{
		return NULL;
	}
	return assert_cast< CTrigger_TractorBeam* >( m_beamHistory.m_beams[0].m_hBeamHandle.Get() );
}


class BlobTraceEnum : public ICountedPartitionEnumerator
{
public:
	BlobTraceEnum( CBaseEntity **pList, int listMax, int contentMask );

	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
	virtual int GetCount() const;
	bool AddToList( CBaseEntity *pEntity );

private:
	CBaseEntity** m_pList;
	int	m_listMax;
	int	m_count;
	int m_contentMask;
};


BlobTraceEnum::BlobTraceEnum( CBaseEntity **pList, int listMax, int contentMask )
	: m_pList( pList ),
	  m_listMax( listMax ),
	  m_count( 0 ),
	  m_contentMask( m_contentMask )
{
}


IterationRetval_t BlobTraceEnum::EnumElement( IHandleEntity *pHandleEntity )
{
#if defined( CLIENT_DLL )
	IClientEntity *pClientEntity = cl_entitylist->GetClientEntityFromHandle( pHandleEntity->GetRefEHandle() );
	C_BaseEntity *pEntity = pClientEntity ? pClientEntity->GetBaseEntity() : NULL;
#else
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
#endif
	if( pEntity )
	{
		// Does this collide with blobs?
		if( ( !pEntity->ShouldCollide( COLLISION_GROUP_PROJECTILE, m_contentMask ) ) /*&& bNotPortalOrTBeam*/ )
			return ITERATION_CONTINUE;

		if( !AddToList( pEntity ) )
			return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}


int BlobTraceEnum::GetCount() const
{
	return m_count;
}


bool BlobTraceEnum::AddToList( CBaseEntity *pEntity )
{
	if( m_count >= m_listMax )
	{
		AssertMsgOnce( 0, "reached enumerated list limit.  Increase limit, decrease radius, or make it so entity flags will work for you" );
		return false;
	}
	m_pList[m_count++] = pEntity;
	return true;
}


BlobTraceResult CBasePaintBlob::BlobHitSolid( CBaseEntity* pHitEntity )
{
	if ( !pHitEntity )
		return BLOB_TRACE_HIT_NOTHING;

	if( pHitEntity->IsWorld() )
	{
		return BLOB_TRACE_HIT_WORLD;
	}
	
	// Player
	if( pHitEntity->IsPlayer() )
	{
		// If the blob started in the player box, it didn't hit. Otherwise, it did.
		// Compensate for updating out-of-sync by sweeping the box along the
		// displacement for the frame.
		Vector mins = pHitEntity->GetAbsOrigin() + pHitEntity->WorldAlignMins();
		Vector maxs = pHitEntity->GetAbsOrigin() + pHitEntity->WorldAlignMaxs();
		const Vector frameDisplacement = pHitEntity->GetAbsVelocity() * gpGlobals->frametime;
		ExpandAABB( mins, maxs, -frameDisplacement );

		// Blobs can only collide if player painting is enabled
		const bool usePaintEffects = player_can_use_painted_power.GetBool() || player_paint_effects_enabled.GetBool();
		const bool canCollideWithPlayer = usePaintEffects && !IsPointInBounds( m_vecPosition, mins, maxs );

		return canCollideWithPlayer ? BLOB_TRACE_HIT_PLAYER : BLOB_TRACE_HIT_NOTHING;
	}

	return BLOB_TRACE_HIT_SOMETHING;	
}


void UTIL_Blob_TraceWorldAndStaticPropsOnly( const Ray_t& ray, trace_t& tr )
{
	CTraceFilterWorldAndPropsOnly traceFilter;
	UTIL_TraceRay( ray, BLOB_TRACE_STATIC_MASK, &traceFilter, &tr );
}


void UTIL_Blob_EnumerateEntitiesAlongRay( const Ray_t& ray, ICountedPartitionEnumerator* pEntEnum )
{
#ifdef GAME_DLL
	if( ray.m_Delta.IsZeroFast() )
		::partition->EnumerateElementsAtPoint( PARTITION_SERVER_GAME_EDICTS, ray.m_Start, false, pEntEnum );
	else
		::partition->EnumerateElementsAlongRay( PARTITION_SERVER_GAME_EDICTS, ray, false, pEntEnum );
#else
	if( ray.m_Delta.IsZeroFast() )
		::partition->EnumerateElementsAtPoint( PARTITION_CLIENT_GAME_EDICTS, ray.m_Start, false, pEntEnum );
	else
		::partition->EnumerateElementsAlongRay( PARTITION_CLIENT_GAME_EDICTS, ray, false, pEntEnum );
#endif
}


bool CBasePaintBlob::CheckCollisionBoxAgainstWorldAndStaticProps()
{
	const float flBoxSize = paintblob_collision_box_size.GetFloat();
	Vector vExtents( flBoxSize, flBoxSize, flBoxSize );

	Ray_t ray;
	ray.Init( m_vecPosition, m_vecTempEndPosition, -vExtents, vExtents );
	trace_t tr;
	UTIL_Blob_TraceWorldAndStaticPropsOnly( ray, tr );

	m_vCollisionBoxCenter = m_vecTempEndPosition;

	return tr.DidHit();
}


void CBasePaintBlob::CheckCollisionAgainstWorldAndStaticProps( BlobCollisionRecord& solidHitRecord, float& flHitFraction )
{
	const float flBoxSize = paintblob_collision_box_size.GetFloat();
	Vector vExtents( flBoxSize, flBoxSize, flBoxSize );
	// if blob moves outside the collision box, recompute the new collision box
	if ( !IsPointInBounds( m_vecTempEndPosition, m_vCollisionBoxCenter - vExtents, m_vCollisionBoxCenter + vExtents ) )
	{
		m_bCollisionBoxHitSolid = CheckCollisionBoxAgainstWorldAndStaticProps();
	}

	// always check if we are hitting world this frame if the flag is set to true
	if ( m_bCollisionBoxHitSolid )
	{
		Ray_t ray;
		ray.Init( m_vecPosition, m_vecTempEndPosition );
		trace_t tr;
		UTIL_ClearTrace( tr );
		UTIL_Blob_TraceWorldAndStaticPropsOnly( ray, tr );

		if ( tr.DidHit() && tr.fraction < flHitFraction )
		{
			flHitFraction = tr.fraction;

			solidHitRecord.trace = tr;
			solidHitRecord.traceResultType = BlobHitSolid( tr.m_pEnt );
			solidHitRecord.targetEndPos = tr.endpos;
		}
	}
}


int CBasePaintBlob::CheckCollision( BlobCollisionRecord *pCollisions, int maxCollisions, const Vector &vecEndPos )
{
	const Vector& vecStartPos = m_vecPosition;

	Ray_t fastRay;
	fastRay.Init( vecStartPos, vecEndPos );

	float flTempFraction = 1.f;
	if ( UTIL_Portal_FirstAlongRay( fastRay, flTempFraction ) == NULL )
	{
		CBaseEntity* ppEntities[ MAX_BLOB_TRACE_ENTITY_RESULTS ];
		BlobTraceEnum entEnum( ppEntities, ARRAYSIZE( ppEntities ), BLOB_TRACE_DYNAMIC_MASK );
		UTIL_Blob_EnumerateEntitiesAlongRay( fastRay, &entEnum );

		int nEntAlongRay = entEnum.GetCount();

		int nCollisionCount = 0;

		float firstHitSolidFraction = 1.f;
		BlobCollisionRecord solidHitRecord;

		// check against world and static props if needed
		CheckCollisionAgainstWorldAndStaticProps( solidHitRecord, firstHitSolidFraction );

		for ( int i=0; i<nEntAlongRay; ++i )
		{
			CBaseEntity* pHitEntity = ppEntities[i];

			if ( FClassnameIs( pHitEntity, "prop_portal" ) )
			{
				pCollisions[nCollisionCount].traceResultType = BLOB_TRACE_HIT_PROP_PORTAL;
				pCollisions[nCollisionCount].trace.m_pEnt = pHitEntity;
				pCollisions[nCollisionCount++].targetEndPos = vecEndPos;
			}
			else if ( FClassnameIs( pHitEntity, "trigger_tractorbeam" ) )
			{
				pCollisions[nCollisionCount].traceResultType = BLOB_TRACE_HIT_TRACTORBEAM;
				pCollisions[nCollisionCount].trace.m_pEnt = pHitEntity;
				pCollisions[nCollisionCount++].targetEndPos = vecEndPos;
			}
			else if ( FClassnameIs( pHitEntity, "trigger_paint_cleanser" ) )
			{
				trace_t tempTrace;
				enginetrace->ClipRayToEntity( fastRay, BLOB_TRACE_DYNAMIC_MASK, pHitEntity, &tempTrace );

				CTriggerPaintCleanser *pPaintCleanser = assert_cast< CTriggerPaintCleanser* >( pHitEntity );
				if ( pPaintCleanser->IsEnabled() && tempTrace.DidHit() && tempTrace.fraction < firstHitSolidFraction )
				{
					firstHitSolidFraction = tempTrace.fraction;

					solidHitRecord.trace = tempTrace;
					solidHitRecord.traceResultType = BLOB_TRACE_HIT_PAINT_CLEANSER;
					solidHitRecord.targetEndPos = tempTrace.endpos;
				}
			}
			// entity that stops the blob
			else if ( pHitEntity->IsSolid() )
			{
				trace_t tempTrace;
				enginetrace->ClipRayToEntity( fastRay, BLOB_TRACE_DYNAMIC_MASK, pHitEntity, &tempTrace );

				if ( tempTrace.DidHit() && tempTrace.fraction < firstHitSolidFraction )
				{
					BlobTraceResult traceResultType = BlobHitSolid( pHitEntity ); 					

					if( traceResultType != BLOB_TRACE_HIT_NOTHING )
					{
						firstHitSolidFraction = tempTrace.fraction;

						solidHitRecord.trace = tempTrace;
						solidHitRecord.traceResultType = traceResultType;
						solidHitRecord.targetEndPos = tempTrace.endpos;
					}
				}
			}
		}

		// add solid collision
		if ( firstHitSolidFraction < 1.f )
		{
			pCollisions[nCollisionCount++] = solidHitRecord;
		}

		return nCollisionCount;
	}
	else
	{
		return CheckCollisionThroughPortal( pCollisions, maxCollisions, vecEndPos );
	}
}


int CBasePaintBlob::CheckCollisionThroughPortal( BlobCollisionRecord *pCollisions, int maxCollisions, const Vector &vecEndPos )
{
	VPROF_BUDGET( "CBasePaintBlob::CheckCollision", VPROF_BUDGETGROUP_PAINTBLOB );
	if( !pCollisions )
		return 0;

	const Vector& vecStartPos = m_vecPosition;
	CTraceFilterHitAll traceFilter;

	// Reserve space for all the output
	CBaseEntity* hitEntities[MAX_BLOB_TRACE_ENTITY_RESULTS];
	int segmentIndices[MAX_BLOB_TRACE_ENTITY_RESULTS];
	int segmentCount = 0;
	const int MAX_BLOB_TRACE_SEGMENTS = 16;
	ComplexPortalTrace_t traceSegments[MAX_BLOB_TRACE_SEGMENTS];
	const int contentMask = BLOB_TRACE_DYNAMIC_MASK | BLOB_TRACE_STATIC_MASK;

	BlobTraceEnum traceEnum( hitEntities, ARRAYSIZE( hitEntities ), contentMask );

	// Run a complex trace for all entities along the ray
	Ray_t blobRay;
	blobRay.Init( vecStartPos, vecEndPos );

	UTIL_Portal_EntitiesAlongRayComplex( segmentIndices,
										 &segmentCount,
										 MIN( MAX_BLOB_TRACE_ENTITY_RESULTS, maxCollisions ),
										 traceSegments,
										 ARRAYSIZE( traceSegments ),
										 blobRay,
										 &traceEnum,
										 &traceFilter,
										 contentMask );

	// Compute the fraction of the total trace that each segment makes up
	float traceSegmentFractions[MAX_BLOB_TRACE_SEGMENTS];
	float traceTravelledLength = 0.0f;
	for( int i = 0; i < segmentCount; ++i )
	{
		const float segLength = (traceSegments[i].trSegment.endpos - traceSegments[i].trSegment.startpos).Length();
		traceSegmentFractions[i] = segLength;
		traceTravelledLength += segLength;
	}

	const float traceTargetLength = blobRay.m_Delta.Length();
	const float invTraceTargetLength = (traceTargetLength > 0.0f) ? (1.0f / traceTargetLength) : (0.0f);
	const float invTraceTravelledLength = (traceTravelledLength > 0.0f) ? (1.0f / traceTravelledLength) : (0.0f);
	for( int i = 0; i < segmentCount; ++i )
	{
		traceSegmentFractions[i] *= invTraceTravelledLength;
	}

	// Compute the fraction so far at each index
	float traceSegmentFractionSoFar[MAX_BLOB_TRACE_SEGMENTS];
	{
		traceSegmentFractionSoFar[0] = 0.0f;
		float fractionSoFar = 0.0f;
		for( int i = 1; i < segmentCount; ++i )
		{
			fractionSoFar += traceSegmentFractions[i - 1];
			traceSegmentFractionSoFar[i] = fractionSoFar;
		}
	}

	// Find which portals were hit and keep track of the last one to test against the world
	// Note: The trace only ends when it hits the world or gets to the end point.
	// Note: Technically, these don't all necessarily need to be processed, but if the blob
	//		 hits something and portals in the same frame, its position doesn't matter.
	int writeIndex = 0;
	CPortal_Base2D* pLastHitPortal = NULL;
	const int lastSegIndex = segmentCount - 1;
	for( int i = 0; i < segmentCount; ++i )
	{
		CPortal_Base2D* pEndPortal = traceSegments[i].pSegmentEndPortal;
		if( pEndPortal != NULL &&
			pEndPortal != pLastHitPortal &&
			pEndPortal->IsActivedAndLinked() )
		{
			pLastHitPortal = pEndPortal;

			pCollisions[writeIndex].traceResultType = BLOB_TRACE_HIT_PORTAL;
			pCollisions[writeIndex].trace = traceSegments[i].trSegment;
			pCollisions[writeIndex].trace.m_pEnt = pEndPortal;
			pCollisions[writeIndex++].targetEndPos = traceSegments[lastSegIndex].trSegment.endpos;
		}
	}


	if ( segmentCount > 1 )
	{
		SetBlobTeleportedThisFrame( true );

#ifdef GAME_DLL
		// record the new server blob teleportation history
		CPaintBlob *pBlob = assert_cast< CPaintBlob* >( this );
		if ( pBlob )
		{
			const float flDeltaTime = gpGlobals->curtime - m_flLastUpdateTime;
			for ( int i=0; i<lastSegIndex; ++i )
			{
				CPortal_Base2D* pEndPortal = traceSegments[i].pSegmentEndPortal;
				const VMatrix& matSourceToLinked = pEndPortal->MatrixThisToLinked();
				const VMatrix& matLinkedToSource = pEndPortal->m_hLinkedPortal->MatrixThisToLinked();
				const Vector& vEnter = traceSegments[ i ].trSegment.endpos;
				const Vector& vExit = traceSegments[ i + 1 ].trSegment.startpos;
				float flTraceFraction = traceSegmentFractionSoFar[i] + traceSegmentFractions[i];
				pBlob->AddBlobTeleportationHistory( BlobTeleportationHistory_t( matSourceToLinked, matLinkedToSource, vEnter, vExit, m_flLastUpdateTime + flTraceFraction * flDeltaTime ) );
			}
		}
#endif
	}

	AssertMsg( !pLastHitPortal || DotProduct( traceSegments[lastSegIndex].trSegment.endpos, pLastHitPortal->m_hLinkedPortal->m_plane_Origin.normal ) > pLastHitPortal->m_hLinkedPortal->m_plane_Origin.dist, "Teleporting blobs behind portal." );

	// Check for water (no entity on the client)
	const float INVALID_TRACE_FRACTION = 2.0f;
	float firstHitSolidFraction = INVALID_TRACE_FRACTION;
	BlobCollisionRecord solidHitRecord;

	// HACK: Non-solid entities with bone followers will be rejected in the enumerator
	//		 but not in the trace. Add the entity to the array anyway.
	int entityCount = traceEnum.GetCount();
	CBaseEntity* pLastTraceEntity = traceSegments[lastSegIndex].trSegment.m_pEnt;
	if( segmentCount > 0 &&
		pLastTraceEntity &&
		!pLastTraceEntity->IsWorld() &&
		pLastTraceEntity != hitEntities[entityCount] &&
		entityCount < ARRAYSIZE( hitEntities ) )
	{
		hitEntities[entityCount] = pLastTraceEntity;
		segmentIndices[entityCount++] = lastSegIndex;
	}

	// Output the rest of the collision records
	for( int i = 0; i < entityCount; ++i )
	{
		CBaseEntity* pHitEntity = hitEntities[i];
		const int segIndex = segmentIndices[i];
		BlobTraceResult traceResultType = BLOB_TRACE_HIT_NOTHING;
		bool bIsLastSegment = ( segIndex == lastSegIndex );

		if ( !pHitEntity )
			continue;

		if ( FClassnameIs( pHitEntity, "trigger_tractorbeam" ) )
		{
			// we don't care about this beam if it's not hitting in the last segment
			if ( !bIsLastSegment )
				continue;

			pCollisions[writeIndex].traceResultType = BLOB_TRACE_HIT_TRACTORBEAM;
			pCollisions[writeIndex].trace = traceSegments[segIndex].trSegment;
			pCollisions[writeIndex].trace.m_pEnt = pHitEntity;
			pCollisions[writeIndex++].targetEndPos = traceSegments[lastSegIndex].trSegment.endpos;

			continue;
		}
		else if ( FClassnameIs( pHitEntity, "prop_portal" ) )
		{
			// we don't care about this portal if it's not hitting in the last segment
			if ( !bIsLastSegment )
				continue;

			pCollisions[writeIndex].traceResultType = BLOB_TRACE_HIT_PROP_PORTAL;
			pCollisions[writeIndex].trace = traceSegments[segIndex].trSegment;
			pCollisions[writeIndex].trace.m_pEnt = pHitEntity;
			pCollisions[writeIndex++].targetEndPos = traceSegments[lastSegIndex].trSegment.endpos;

			continue;
		}
		else if ( FClassnameIs( pHitEntity, "trigger_paint_cleanser" ) )
		{
			trace_t tempTrace;
			Ray_t traceRay;
			traceRay.Init( traceSegments[segIndex].trSegment.startpos, traceSegments[segIndex].trSegment.endpos );
			enginetrace->ClipRayToEntity( traceRay, BLOB_TRACE_DYNAMIC_MASK, pHitEntity, &tempTrace );

			float fraction = traceSegmentFractionSoFar[segIndex] + traceSegmentFractions[segIndex] * tempTrace.fraction;

			CTriggerPaintCleanser *pPaintCleanser = assert_cast< CTriggerPaintCleanser* >( pHitEntity );
			if ( pPaintCleanser->IsEnabled() && tempTrace.DidHit() && fraction < firstHitSolidFraction )
			{
				firstHitSolidFraction = fraction;
				solidHitRecord.trace = tempTrace;
				solidHitRecord.traceResultType = BLOB_TRACE_HIT_PAINT_CLEANSER;
				solidHitRecord.targetEndPos = tempTrace.endpos;
			}

			continue;
		}
		// Any other entity
		else if ( pHitEntity->IsSolid() )
		{
			traceResultType = BlobHitSolid( pHitEntity );
			if( traceResultType == BLOB_TRACE_HIT_NOTHING )
				continue;
		}

		// Find the actual intersection information and fraction along the total trace
		float fraction = 1.0f;
		trace_t trace;
		UTIL_ClearTrace( trace );

		// This entity stopped the trace
		if( pHitEntity == traceSegments[segIndex].trSegment.m_pEnt )
		{
			trace = traceSegments[segIndex].trSegment;
			
			// Here, the trace hit the entity, so there's no need to recompute the intersection
			// The fraction in the trace is the fraction of the remaining ray delta.
			const float totalFractionSoFar = traceSegmentFractionSoFar[segIndex] * traceTravelledLength * invTraceTargetLength;
			const float remainingLength = traceTargetLength - totalFractionSoFar * traceTargetLength;
			const float remainingLengthTravelled = trace.fraction * remainingLength;
			fraction = totalFractionSoFar + remainingLengthTravelled * invTraceTargetLength;
		}
		// This entity was somewhere along its corresponding trace segment
		else if ( pHitEntity->IsSolid() )
		{
			Ray_t traceRay;
			traceRay.Init( traceSegments[segIndex].trSegment.startpos, traceSegments[segIndex].trSegment.endpos );
			enginetrace->ClipRayToEntity( traceRay, contentMask, pHitEntity, &trace );
			fraction = traceSegmentFractionSoFar[segIndex] + traceSegmentFractions[segIndex] * trace.fraction;
		}

		// If the trace is valid and the best so far, record the collision
		if( trace.DidHit() && trace.m_pEnt && fraction <= firstHitSolidFraction )
		{
			firstHitSolidFraction = fraction;
			solidHitRecord.trace = trace;
			solidHitRecord.traceResultType = traceResultType;

			// Use the end position of this segment as the target end position.
			solidHitRecord.targetEndPos = traceSegments[segIndex].trSegment.endpos;
		}
	}

	if( firstHitSolidFraction != INVALID_TRACE_FRACTION )
	{
		pCollisions[writeIndex++] = solidHitRecord;
	}

	return writeIndex;
}


void CBasePaintBlob::ResolveCollision( bool& bDeleted, const BlobCollisionRecord& collision, Vector& targetVelocity, float deltaTime )
{
	VPROF_BUDGET( "CBasePaintBlob::ResolveCollision", VPROF_BUDGETGROUP_PAINTBLOB );

	//Check what the blob hit
	switch( collision.traceResultType )
	{
		case BLOB_TRACE_HIT_PORTAL:
			//If the blob went through a portal then teleport it out of the portal
			{
				if( m_MoveState != PAINT_BLOB_STREAK_MOVE )
				{
					CPortal_Base2D* pInPortal = assert_cast<CPortal_Base2D*>( collision.trace.m_pEnt );
					//If the portal is active and linked
					if( pInPortal && pInPortal->IsActivedAndLinked() )
					{
						PaintBlobMoveThroughPortal( deltaTime, pInPortal, collision.trace.startpos, collision.targetEndPos );
						targetVelocity = GetVelocity();
					}
				}
				else
				{
					SetDeletionFlag( true );
					bDeleted = true;
				}
			}
			break;
		case BLOB_TRACE_HIT_TRACTORBEAM:
			{
				CTrigger_TractorBeam* pBeam = assert_cast< CTrigger_TractorBeam* >( collision.trace.m_pEnt );
				if ( pBeam )
				{
					SetTractorBeam( pBeam );

					SetPosition( collision.targetEndPos );
					SetVelocity( targetVelocity );
					m_bInTractorBeam = true;
				}
			}
			break;
		case BLOB_TRACE_HIT_PROP_PORTAL:
			{
				SetPosition( collision.targetEndPos );
				SetVelocity( targetVelocity );

				CProp_Portal *pPortal = assert_cast< CProp_Portal* >( collision.trace.m_pEnt );
				if ( pPortal && pPortal->IsActivedAndLinked() )
				{
					m_hPortal = pPortal;
				}
			}
			break;
		case BLOB_TRACE_HIT_PAINT_CLEANSER:
			{
				PlayEffect( collision.targetEndPos, collision.trace.plane.normal );
				SetDeletionFlag( true );
				bDeleted = true;
			}
			break;
		case BLOB_TRACE_HIT_WORLD:
		case BLOB_TRACE_HIT_SOMETHING:
		case BLOB_TRACE_HIT_PLAYER:
			//If the blob hit something
			{
				SetVelocity( targetVelocity );

				//Remove the blob if the blob should not streak on this surface
				bDeleted = !PaintBlobCheckShouldStreak( collision.trace ); 
			}
			break;
	}
}


void CBasePaintBlob::UpdateBlobCollision( float flDeltaTime, const Vector& vecEndPos, Vector& vecEndVelocity )
{
	//Debugging flags
	bool bDebuggingBlobs = false;

#ifdef CLIENT_DLL
	if( debug_paint_client_blobs.GetBool() )
#else
	if( debug_paint_server_blobs.GetBool() )
#endif
	{
		bDebuggingBlobs = true;
	}

	// disable blob particles and blob render if the blob is streaking
	if ( m_MoveState == PAINT_BLOB_STREAK_MOVE && !paintblob_streak_particles_enabled.GetBool() )
	{
		m_bSilent = true;
	}

	//Check if the blob collided with anything
	BlobCollisionRecord collisions[MAX_BLOB_TRACE_ENTITY_RESULTS];

	int collisionCount = CheckCollision( collisions, MAX_BLOB_TRACE_ENTITY_RESULTS, vecEndPos );

	//Draw a tracer line to show the blobs path
	if( bDebuggingBlobs )
	{
		NDebugOverlay::Line( GetPrevPosition(), vecEndPos, g_BlobDebugColor.r(), g_BlobDebugColor.g(), g_BlobDebugColor.b(), false, 10.0f );
		NDebugOverlay::VertArrow( GetPrevPosition(), vecEndPos, 4.0f, 0, 255, 0, 255, true, 0.01f );
	}

	// reset state
	m_bInTractorBeam = false;
	ResetGhostState();

	CTrigger_TractorBeam *pPreviousCurrentBeam = GetCurrentBeam();

	//If the blob didn't touch anything then move it
	if( collisionCount == 0 )
	{
		//Move the blob to its end pos
		SetPosition( vecEndPos );
		SetVelocity( vecEndVelocity );
		if ( m_MoveState != PAINT_BLOB_STREAK_MOVE )
		{
			SetMoveState( PAINT_BLOB_AIR_MOVE );
		}
	}
	// If the blob collided with things, resolve the collisions
	else
	{
		bool bDeleted = false;
		for( int i = 0; i < collisionCount && !bDeleted; ++i )
		{
			ResolveCollision( bDeleted, collisions[i], vecEndVelocity, flDeltaTime );
		}
	}

	// reset beam if need
	if ( !m_bInTractorBeam )
	{
		SetTractorBeam( NULL );
	}
	else if ( pPreviousCurrentBeam != GetCurrentBeam() )
	{
		// add blobs to beam
		GetCurrentBeam()->m_blobs.AddToTail( assert_cast< CPaintBlob* >( this ) );
	}
}


void CBasePaintBlob::UpdateBlobPostCollision( float flDeltaTime )
{
	//If the paint blob is streaking
	if( m_MoveState == PAINT_BLOB_STREAK_MOVE )
	{
		Vector vVelocity = m_vecVelocity;

		// just remove the blob if the it's trying to streak with speed == 0
		if ( vVelocity.IsZero() )
		{
			SetDeletionFlag( true );
			return;
		}

		//Update the streak timer
		m_flStreakTimer -= flDeltaTime;

		// apply streak paint
		bool bDeleted = PaintBlobStreakPaint( m_vecPosition );

		//Dampen the speed of the blobs while streaking
		float flSpeed = VectorNormalize( vVelocity );
		flSpeed -= m_flStreakSpeedDampenRate * flDeltaTime;

		//If the blob should still be streaking
		if( !bDeleted && m_flStreakTimer >= 0.0f && flSpeed >= 0.0f )
		{
			SetVelocity( vVelocity * flSpeed );

			//Reset the streak dir changed flag
			m_bStreakDirChanged = false;
		}
		else
		{
			SetDeletionFlag( true );
			return;
		}
	}


	//Reset the move state
	if ( m_MoveState != PAINT_BLOB_TRACTOR_BEAM_MOVE )
	{
		DecayVortexSpeed( flDeltaTime );
	}
}


void CBasePaintBlob::PaintBlobMoveThroughPortal( float flDeltaTime, CPortal_Base2D *pInPortal, const Vector &vecStartPos, const Vector &vecTransformedEndPos )
{
	VMatrix matTransform = pInPortal->MatrixThisToLinked();
	Vector vTransfromedVelocity;
	UTIL_Portal_VectorTransform( matTransform, m_vecVelocity, vTransfromedVelocity );
	SetVelocity( vTransfromedVelocity );

	SetPosition( vecTransformedEndPos );

	//Make sure the blobs have a min velocity when coming out of a portal
	float flMinVelocity = paintblob_minimum_portal_exit_velocity.GetFloat();

	if( m_vecVelocity.LengthSqr() < ( flMinVelocity * flMinVelocity ) )
	{
		m_vecVelocity.NormalizeInPlace();
		SetVelocity( m_vecVelocity * flMinVelocity );
	}

	// increment blob teleportation count
	++m_nTeleportationCount;

	SetMoveState( PAINT_BLOB_AIR_MOVE );
}


bool CBasePaintBlob::PaintBlobCheckShouldStreak( const trace_t &trace )
{
	bool bDebuggingStreaking = false;
#ifdef GAME_DLL
	if( debug_paintblobs_streaking.GetBool() )
	{
		bDebuggingStreaking = true;
	}
#endif

	// don't streak if blob is in tractor beam or out of streak time
	if( m_flStreakTimer <= 0.0f || m_bInTractorBeam || m_MoveState == PAINT_BLOB_TRACTOR_BEAM_MOVE )
	{
		CPaintBlob *pBlob = assert_cast< CPaintBlob* >( this );
		pBlob->PaintBlobPaint( trace );
		SetDeletionFlag( true );
		return false;
	}


	if( bDebuggingStreaking )
	{
		//Draw the collision position
		NDebugOverlay::Cross3D( trace.startpos, 2.0f, 255 - g_BlobDebugColor.r(), 255 - g_BlobDebugColor.g(), 255 - g_BlobDebugColor.b(), true, 10.0f );
		NDebugOverlay::Cross3D( trace.endpos, 2.0f, g_BlobDebugColor.r(), g_BlobDebugColor.g(), g_BlobDebugColor.b(), true, 10.0f );
	}

	const Vector& vecSurfaceNormal = trace.plane.normal;
	Vector vecStreakVelocity = m_vecVelocity - DotProduct( vecSurfaceNormal, m_vecVelocity ) * vecSurfaceNormal;

	Vector vecVelocityDir = m_vecVelocity.Normalized();
	Vector vecStreakVelocityDir = vecStreakVelocity.Normalized();

	//Check the angle of impact for the blob
	float flBlobImpactAngle = RAD2DEG( acos( clamp( DotProduct( vecStreakVelocityDir, vecVelocityDir ), -1.f, 1.f ) ) );

	if( bDebuggingStreaking )
	{
		Vector vecDrawPos = trace.endpos + vecSurfaceNormal * 50;

		//Draw the surface normal
		NDebugOverlay::VertArrow( vecDrawPos, vecDrawPos + vecSurfaceNormal * 50, 2, 0, 255, 0, 255, true, 10.0f );

		//Draw the velocity dir of the blob
		NDebugOverlay::VertArrow( vecDrawPos, vecDrawPos + (vecVelocityDir * 50), 2, 0, 0, 255, 255, true, 10.0f );

		//Draw the streak velocity of the blob
		NDebugOverlay::VertArrow( vecDrawPos, vecDrawPos + vecStreakVelocityDir * 50, 2, 0, 255, 255, 255, true, 10.0f );

		//Display the impact angle of the blob
		CFmtStr msg;
		msg.sprintf( "Impact angle: %f\n", flBlobImpactAngle );
		NDebugOverlay::Text( vecDrawPos, msg, true, 10.0f );
	}

	bool bShouldStreak = false;

	//Check the streaking conditions
	if( ( vecSurfaceNormal.z > -0.5f && vecSurfaceNormal.z < 0.5f ) || //If the blob hit a wall
		flBlobImpactAngle <= paintblob_streak_angle_threshold.GetFloat() ) //If the impact angle of the blob is within the threshold
	{
		bShouldStreak = true;
	}

	//Check if the blob should streak paint
	Vector vecHitPlaneDir = trace.plane.normal.Normalized();
	bool bSameSurface = AlmostEqual( -m_vecStreakDir, vecHitPlaneDir );

	//Set the streaking data if
	CPaintBlob *pBlob = assert_cast< CPaintBlob* >( this );
	if( ( m_MoveState != PAINT_BLOB_STREAK_MOVE && bShouldStreak ) || //The blob was not already streaking and it should streak
		( m_MoveState == PAINT_BLOB_STREAK_MOVE && bShouldStreak && !bSameSurface ) )//The blob was streaking and is should streak on a different surface
	{
		//Set the streaking data for the blob
		SetMoveState( PAINT_BLOB_STREAK_MOVE );
		SetVelocity( vecStreakVelocity );
		pBlob->PaintBlobPaint( trace );
		m_vecStreakDir = -( vecHitPlaneDir );

		return true;
	}

	pBlob->PaintBlobPaint( trace );
	SetDeletionFlag( true );

	return bShouldStreak;
}


bool CBasePaintBlob::PaintBlobStreakPaint( const Vector &vecBlobStartPos )
{
	bool bRemoveBlob = false;

	Ray_t blobRay;
	blobRay.Init( vecBlobStartPos, vecBlobStartPos + m_vecStreakDir * paintblob_streak_trace_range.GetFloat() );

#ifndef CLIENT_DLL
	if( debug_paintblobs_streaking.GetBool() )
	{
		NDebugOverlay::Line( vecBlobStartPos, vecBlobStartPos + m_vecStreakDir * paintblob_streak_trace_range.GetFloat(), 255, 0, 255, true, 10.0f );
	}
#endif

	//See if the blob hit a portal or anything else
	trace_t trace;
	CTraceFilterNoPlayers traceFilter;
	const int contentMask = BLOB_TRACE_STATIC_MASK | BLOB_TRACE_DYNAMIC_MASK;
	CPortal_Base2D *pInPortal = UTIL_Portal_TraceRay( blobRay, contentMask, &traceFilter, &trace );

	//If the blob hit a portal
	if( pInPortal )
	{
		bRemoveBlob = true;
	}
	//If the blob hit something else besides the player
	else if( trace.DidHit() )
	{
		PaintBlobPaint( trace );
	}
	else //The blob hit nothing
	{
		//Don't remove the blob if the streak direction changed this update
		if( !m_bStreakDirChanged )
		{
			bRemoveBlob = true;
		}
	}

	return bRemoveBlob;
}


void CBasePaintBlob::SetTractorBeam( CTrigger_TractorBeam *pBeam )
{
	if ( pBeam == NULL )
	{
		if ( m_MoveState != PAINT_BLOB_STREAK_MOVE )
		{
			SetMoveState( PAINT_BLOB_AIR_MOVE );
		}

		m_beamHistory.ClearAllBeams();

		return;
	}
	
	SetMoveState( PAINT_BLOB_TRACTOR_BEAM_MOVE );
	if ( m_beamHistory.IsDifferentBeam( pBeam ) )
	{
		m_beamHistory.UpdateBeam( pBeam );

		Vector vecPointOnLine;
		CalcClosestPointOnLineSegment( GetPosition(), pBeam->GetStartPoint(), pBeam->GetEndPoint(), vecPointOnLine );
		float flDistFromBeamCenter = ( GetPosition() - vecPointOnLine ).Length();

		float flFraction = 1.f / UTil_Blob_BeamRadiusOffset( pBeam->GetBeamRadius() );
		m_flCurrentVortexRadius = RemapValClamped( flDistFromBeamCenter, 1.f, UTil_Blob_BeamRadiusOffset( pBeam->GetBeamRadius() ), flFraction, 1.f );
	}
}


void CBasePaintBlob::DecayVortexSpeed( float flDeltaTime )
{
	if ( m_flCurrentVortexSpeed > 0.f )
	{
		m_flCurrentVortexSpeed = clamp( m_flCurrentVortexSpeed - flDeltaTime * paintblob_tbeam_vortex_accel.GetFloat(), 0.f, m_flCurrentVortexSpeed );
	}
}


const Vector& CBasePaintBlob::GetContactNormal() const
{
	return m_vContactNormal;
}


void CBasePaintBlob::PlayEffect( const Vector& vPosition, const Vector& vNormal )
{
	SetPosition( vPosition );
	m_vContactNormal = vNormal;
	m_bShouldPlayEffect = true;
}


struct BlobInBeam_t : std::unary_function< CPaintBlob*, bool >
{
	inline bool operator()( const CPaintBlob* pBlob ) const
	{
		return pBlob->GetMoveState() == PAINT_BLOB_TRACTOR_BEAM_MOVE;
	}
};


struct BlobInAir_t : std::unary_function< CPaintBlob*, bool >
{
	inline bool operator()( const CPaintBlob* pBlob ) const
	{
		return pBlob->GetMoveState() == PAINT_BLOB_AIR_MOVE;
	}
};


void SplitBlobsIntoMovementGroup( PaintBlobVector_t& blobs, PaintBlobVector_t& blobsInBeam, PaintBlobVector_t& blobsInAir, PaintBlobVector_t& blobsInStreak )
{
	// split blobs in beam
	CPaintBlob** begin = blobs.Base();
	CPaintBlob** end = begin + blobs.Count();
	CPaintBlob** middle = std::partition( begin, end, BlobInBeam_t() );

	int numBlobsInBeam = middle - begin;
	blobsInBeam.CopyArray( begin, numBlobsInBeam );

	// split blobs in air
	begin = middle;
	middle = std::partition( begin, end, BlobInAir_t() );
	int numBlobsInAir = middle - begin;
	blobsInAir.CopyArray( begin, numBlobsInAir );

	int numBlobsInStreak = end - middle;
	blobsInStreak.CopyArray( middle, numBlobsInStreak );
}


class BlobsInBeamUpdate_SIMD
{
public:
	BlobsInBeamUpdate_SIMD( CTrigger_TractorBeam *pBeam )
	{
		const PaintBlobVector_t& blobs = pBeam->m_blobs;

		if ( blobs.Count() == 0 )
			return;

		m_flDeltaTime = gpGlobals->curtime - blobs[0]->GetLastUpdateTime();
		m_pBeam = pBeam;

		int numBlobs = blobs.Count();
		m_data.EnsureCount( numBlobs );
		for ( int i=0; i<numBlobs; ++i )
		{
			m_data[i].m_vPosition = blobs[i]->GetPosition();
			m_data[i].m_vVelocity = blobs[i]->GetVelocity();
			m_data[i].m_flCurrentVortexRadius = blobs[i]->m_flCurrentVortexRadius;
			m_data[i].m_flCurrentVortexSpeed = blobs[i]->m_flCurrentVortexSpeed;
			m_data[i].m_flDestVortexRadius = blobs[i]->m_flDestVortexRadius;
			m_data[i].m_flVortexDirection = blobs[i]->m_flVortexDirection;

#ifdef BLOB_SIMD_DEBUG
			m_data[i].m_vPosition_DEBUG = blobs[i]->GetPosition();
			m_data[i].m_vVelocity_DEBUG = blobs[i]->GetVelocity();
			m_data[i].m_flCurrentVortexRadius_DEBUG = blobs[i]->m_flCurrentVortexRadius;
			m_data[i].m_flCurrentVortexSpeed_DEBUG = blobs[i]->m_flCurrentVortexSpeed;
#endif
		}

		UpdateBlobsInBeam_SIMD();

		for ( int i=0; i<numBlobs; ++i )
		{
			blobs[i]->SetTempEndPosition( m_data[i].m_vPosition );
			blobs[i]->SetTempEndVelocity( m_data[i].m_vVelocity );
			blobs[i]->m_flCurrentVortexRadius = m_data[i].m_flCurrentVortexRadius;
			blobs[i]->m_flCurrentVortexSpeed = m_data[i].m_flCurrentVortexSpeed;
		}
	}

	~BlobsInBeamUpdate_SIMD()
	{
		m_data.Purge();
	}
private:
	struct BlobBeamUpdateData_t
	{
		Vector m_vPosition;
		Vector m_vVelocity;
		
		// beam info
		float m_flCurrentVortexRadius;
		float m_flCurrentVortexSpeed;

		float m_flDestVortexRadius;
		float m_flVortexDirection;
		
#ifdef BLOB_SIMD_DEBUG
		Vector m_vPosition_DEBUG;
		Vector m_vVelocity_DEBUG;
		float m_flCurrentVortexRadius_DEBUG;
		float m_flCurrentVortexSpeed_DEBUG;
#endif
	};


	void UpdateBlobsInBeam_SIMD()
	{
		const float flDeltaTime = m_flDeltaTime;
		const float flInvDeltaTime = 1.0 / m_flDeltaTime;
		const float flBeamAccelDT = flDeltaTime * paintblob_tbeam_accel.GetFloat();
		const float flVortexAccelDT = flDeltaTime * paintblob_tbeam_vortex_accel.GetFloat();
		const float flVortexDistance = paintblob_tbeam_vortex_distance.GetFloat();
		const float flTbeamCirculation = paintblob_tbeam_vortex_circulation.GetFloat();
		const float flTbeamPortalCirculation = paintblob_tbeam_portal_vortex_circulation.GetFloat();
		const float flVortexRadiusOffsetDT = flDeltaTime * paintblob_tbeam_vortex_radius_rate.GetFloat();
		const float TWO_PI = 2.f * M_PI;

		// beam constant
		const Vector& vBeamStart = m_pBeam->GetStartPoint();
		const Vector& vBeamEnd = m_pBeam->GetEndPoint();
		Vector vBeamDir = vBeamEnd - vBeamStart;
		const float flBeamLength = vBeamDir.NormalizeInPlace();
		const float flHalfBeamLength = 0.5f * flBeamLength;
		bool bIsBeamReversed = m_pBeam->IsReversed();
		bool bGoingTowardsPortal = ( bIsBeamReversed ) ? m_pBeam->IsFromPortal() : m_pBeam->IsToPortal();
		float flBeamRadius = UTil_Blob_BeamRadiusOffset( m_pBeam->GetBeamRadius() );

		const float flMinVortexRadiusScale = 1.f / flBeamRadius;

#ifdef BLOB_SIMD_DEBUG

		const float flBeamRadiusSqr = Square( flBeamRadius );
		const float flBeamSpeed = UTil_Blob_TBeamLinearForce( m_pBeam->GetLinearForce() );

		for ( int j=0; j<m_data.Count(); ++j )
		{
			float flSpeed = DotProduct( m_data[j].m_vVelocity_DEBUG, vBeamDir );

			// if speed < 0, set it to 0
			flSpeed = fsel( flSpeed, flSpeed, 0.f );

			// now clamp the new speed
			const float flNewSpeed = flSpeed + flBeamAccelDT;
			flSpeed = fsel( flNewSpeed - flBeamSpeed, flBeamSpeed, flNewSpeed );

			Vector vecEndVelocity = flSpeed * vBeamDir;
			Vector vecEndPos = m_data[j].m_vPosition_DEBUG + flDeltaTime * vecEndVelocity;

			// vortex, depending on where the blob is a long the beam
			Vector vecPointOnLine;
			float flDistOnLine;
			CalcClosestPointOnLine( vecEndPos, vBeamStart, vBeamEnd, vecPointOnLine, &flDistOnLine );
			flDistOnLine *= flBeamLength;

			// compute circulation, if endpos is within the vortex distance, we should vortex at a faster rate (going through portal)
			const float flDistFromBeamCenter = fabs( flDistOnLine - flHalfBeamLength );
			float flCirculation = fsel( flHalfBeamLength - flDistFromBeamCenter - flVortexDistance, flTbeamCirculation, flTbeamPortalCirculation );

			// current vortex radius
			float flDestVortexRadiusScale = ( flBeamLength - flDistOnLine - flVortexDistance < 0.f && bGoingTowardsPortal ) ? flMinVortexRadiusScale : m_data[j].m_flDestVortexRadius;
			float flDestVortexRadius = flDestVortexRadiusScale * flBeamRadius;
			float flCurrentVortexRadiusScale = m_data[j].m_flCurrentVortexRadius_DEBUG;
			float flCurrentVortexRadius = flCurrentVortexRadiusScale * flBeamRadius;
			
			// current vortex speed
			float flDestVortexSpeed = flCirculation / ( TWO_PI * flCurrentVortexRadius );
			float flCurrentVortexSpeed = m_data[j].m_flCurrentVortexSpeed_DEBUG;

			// compute new vortex radius
			float flSign = Sign( flDestVortexRadius - flCurrentVortexRadius );
			float flNewVortexRadius = flCurrentVortexRadius + flSign * flVortexRadiusOffsetDT;
			flCurrentVortexRadius = fsel( flSign * ( flDestVortexRadius - flNewVortexRadius ), flNewVortexRadius, flDestVortexRadius );

			// compute new vortex speed
			float flVortexSpeedSign = Sign( flDestVortexSpeed - flCurrentVortexSpeed );
			float flNewVortexSpeed = flCurrentVortexSpeed + flVortexSpeedSign * flVortexAccelDT;
			flCurrentVortexSpeed = fsel( flVortexSpeedSign * ( flDestVortexSpeed - flNewVortexSpeed ), flNewVortexSpeed, flDestVortexSpeed );

			// spiral pos around m_vecBeamDir at random speed
			Vector vecVortexRadius = vecEndPos - vecPointOnLine;			
			float flVortexDirection = ( bIsBeamReversed ) ? -m_data[j].m_flVortexDirection : m_data[j].m_flVortexDirection;
			Quaternion qRotate;
			AxisAngleQuaternion(vBeamDir, flDeltaTime * flVortexDirection * flCurrentVortexSpeed, qRotate );
			Vector tempVec = vecVortexRadius;
			VectorRotate( tempVec, qRotate, vecVortexRadius );
			vecVortexRadius = flCurrentVortexRadius * vecVortexRadius.Normalized();

			Vector newPos = vecPointOnLine + vecVortexRadius;
			// add angular velocity
			Vector vAngularVelocity = ( newPos - vecEndPos ) * flInvDeltaTime;

			// scale angular velocity by how far off the center of the beam
			float flDistFromCenterSqr = ( vecPointOnLine - vecEndPos ).LengthSqr();
			float flAngularVelocityScale = RemapValClamped( flDistFromCenterSqr, 0.f, flBeamRadiusSqr, 0.f, 1.f );

			vecEndVelocity += flAngularVelocityScale * vAngularVelocity;

			// Output
			float flFinalRadiusScale = RemapValClamped( flCurrentVortexRadius, 1.f, flBeamRadius, flMinVortexRadiusScale, 1.f );
			m_data[j].m_flCurrentVortexRadius_DEBUG = flFinalRadiusScale;
			m_data[j].m_flCurrentVortexSpeed_DEBUG = flCurrentVortexSpeed;
			m_data[j].m_vVelocity_DEBUG = vecEndVelocity;
			m_data[j].m_vPosition_DEBUG = newPos;
		}
#endif

		// const
		fltx4 f4MinVortexRadiusScale = ReplicateX4( flMinVortexRadiusScale );

		fltx4 f4DeltaTime = ReplicateX4( flDeltaTime );
		fltx4 f4InvDeltaTime = ReplicateX4( flInvDeltaTime );
		fltx4 f4BeamAccelDT = ReplicateX4( flBeamAccelDT );
		fltx4 f4VortexDistance = ReplicateX4( flVortexDistance );
		fltx4 f4TbeamCirculation = ReplicateX4( flTbeamCirculation );
		fltx4 f4TbeamPortalCirculation = ReplicateX4( flTbeamPortalCirculation );
		fltx4 f4VortexRadiusOffsetDT = ReplicateX4( flVortexRadiusOffsetDT );
		fltx4 f4VortexAccelDT = ReplicateX4( flVortexAccelDT );

		// beam info SIMD
		fltx4 f4BeamStart = LoadUnaligned3SIMD( m_pBeam->GetStartPoint().Base() );
		fltx4 f4BeamEnd = LoadUnaligned3SIMD( m_pBeam->GetEndPoint().Base() );
		fltx4 f4BeamDir = SubSIMD( f4BeamEnd, f4BeamStart );

		// compute length and normalize f4BeamDir
		fltx4 scLengthSqr = Dot3SIMD( f4BeamDir, f4BeamDir );
		bi32x4 isSignificant = CmpGtSIMD( scLengthSqr, Four_Epsilons );
		fltx4 scLengthInv = ReciprocalSqrtSIMD( scLengthSqr );
		f4BeamDir = AndSIMD( isSignificant, MulSIMD( f4BeamDir, scLengthInv ) );

		fltx4 f4BeamRadius = ReplicateX4( flBeamRadius );
		fltx4 f4BeamRadiusSqr = MulSIMD( f4BeamRadius, f4BeamRadius );
		fltx4 f4BeamSpeed = ReplicateX4( UTil_Blob_TBeamLinearForce( m_pBeam->GetLinearForce() ) );
		fltx4 f4BeamLength = ReplicateX4( flBeamLength );
		fltx4 f4HalfBeamLength = ReplicateX4( flHalfBeamLength );

		const int32 ALIGN16 maskTRUE[4] ALIGN16_POST  = { ~0, ~0, ~0, ~0 };
		const int32 ALIGN16 maskFALSE[4] ALIGN16_POST = {  0,  0,  0,  0 };
		
		bi32x4 bCmpGoingTowardsPortal = ( bGoingTowardsPortal ) ? (bi32x4)LoadAlignedSIMD( maskTRUE ) : (bi32x4)LoadAlignedSIMD( maskFALSE );
		bi32x4 bCmpIsBeamReversed = ( bIsBeamReversed ) ? (bi32x4)LoadAlignedSIMD( maskTRUE ) : (bi32x4)LoadAlignedSIMD( maskFALSE );

		fltx4 f4TWO_PI = ReplicateX4( TWO_PI );

		FourVectors v4BeamStart = FourVectors( vBeamStart, vBeamStart, vBeamStart, vBeamStart );
		FourVectors v4BeamEnd = FourVectors( vBeamEnd, vBeamEnd, vBeamEnd, vBeamEnd );
		FourVectors v4BeamDir = FourVectors( vBeamDir, vBeamDir, vBeamDir, vBeamDir );

		int count = m_data.Count();
		int i = 0;

		while ( count >= 4 )
		{
			FourVectors v4Velocity = FourVectors( m_data[i].m_vVelocity,
													m_data[i+1].m_vVelocity,
													m_data[i+2].m_vVelocity,
													m_data[i+3].m_vVelocity );

			// dot 4 vectors
			fltx4 f4Speed = v4Velocity * v4BeamDir;

			// if speed < 0, set it to 0
			f4Speed = MaskedAssign( CmpLtSIMD( f4Speed, Four_Zeros ), Four_Zeros, f4Speed );

			// ( f4Speed < f4NewSpeed ) ? f4Speed : f4NewSpeed;
			fltx4 f4NewSpeed = AddSIMD( f4Speed, f4BeamAccelDT );

			// clamp
			f4Speed = MaskedAssign( CmpLtSIMD( f4NewSpeed, f4BeamSpeed ), f4NewSpeed, f4BeamSpeed );

			FourVectors v4EndVelocity = Mul( v4BeamDir, f4Speed );

			FourVectors v4EndPos = FourVectors( m_data[i].m_vPosition,
												m_data[i+1].m_vPosition,
												m_data[i+2].m_vPosition,
												m_data[i+3].m_vPosition );
			v4EndPos = Mul( v4EndVelocity, f4DeltaTime ) + v4EndPos;

			// vortex, depending on where the blob is a long the beam
			FourVectors v4VecPointOnline;
			fltx4 f4DistOnLine;
			FourVectors::CalcClosestPointOnLineSIMD( v4EndPos, v4BeamStart, v4BeamEnd, v4VecPointOnline, &f4DistOnLine );
			f4DistOnLine = MulSIMD( f4BeamLength, f4DistOnLine );

			// compute circulation, if endpos is within the vortex distance, we should vortex at a faster rate (going through portal)
			fltx4 f4DistFromBeamCenter = fabs( SubSIMD( f4DistOnLine, f4HalfBeamLength ) );
			bi32x4 bCmp = CmpLtSIMD( SubSIMD( SubSIMD( f4HalfBeamLength, f4DistFromBeamCenter ), f4VortexDistance ), Four_Zeros );
			fltx4 f4Circulation = MaskedAssign( bCmp, f4TbeamPortalCirculation, f4TbeamCirculation );

			// current vortex radius
			bCmp = CmpLtSIMD( SubSIMD( SubSIMD( f4BeamLength, f4DistOnLine ), f4VortexDistance ), Four_Zeros );
			fltx4 f4TempDestRadius = { m_data[i].m_flDestVortexRadius,
										m_data[i+1].m_flDestVortexRadius,
										m_data[i+2].m_flDestVortexRadius,
										m_data[i+3].m_flDestVortexRadius };
			fltx4 f4DestVortexRadiusScale = MaskedAssign( AndSIMD( bCmp, bCmpGoingTowardsPortal ), f4MinVortexRadiusScale, f4TempDestRadius );
			fltx4 f4DestVortexRadius = MulSIMD( f4DestVortexRadiusScale, f4BeamRadius );
			fltx4 f4CurrentVortexRadiusScale = { m_data[i].m_flCurrentVortexRadius,
												m_data[i+1].m_flCurrentVortexRadius,
												m_data[i+2].m_flCurrentVortexRadius,
												m_data[i+3].m_flCurrentVortexRadius };
			fltx4 f4CurrentVortexRadius = MulSIMD( f4CurrentVortexRadiusScale, f4BeamRadius );

			// current vortex speed
			fltx4 f4DestVortexSpeed = DivEstSIMD( f4Circulation, MulSIMD( f4TWO_PI, f4CurrentVortexRadius ) );
			fltx4 f4CurrentVortexSpeed = { m_data[i].m_flCurrentVortexSpeed,
											m_data[i+1].m_flCurrentVortexSpeed,
											m_data[i+2].m_flCurrentVortexSpeed,
											m_data[i+3].m_flCurrentVortexSpeed };

			// compute new vortex radius
			fltx4 f4RadiusSign = MaskedAssign( CmpLtSIMD( SubSIMD( f4DestVortexRadius, f4CurrentVortexRadius ), Four_Zeros ), Four_NegativeOnes, Four_Ones );
			fltx4 f4NewVortexRadius = MaddSIMD( f4RadiusSign, f4VortexRadiusOffsetDT, f4CurrentVortexRadius );
			bCmp = CmpGtSIMD( MulSIMD( f4RadiusSign, SubSIMD( f4DestVortexRadius, f4NewVortexRadius ) ), Four_Zeros );
			f4CurrentVortexRadius = MaskedAssign( bCmp, f4NewVortexRadius, f4DestVortexRadius );

			// compute new vortex speed
			fltx4 f4VortexSpeedSign = MaskedAssign( CmpLtSIMD( SubSIMD( f4DestVortexSpeed, f4CurrentVortexSpeed ), Four_Zeros ), Four_NegativeOnes, Four_Ones );
			fltx4 f4NewVortexSpeed = MaddSIMD( f4VortexSpeedSign, f4VortexAccelDT, f4CurrentVortexSpeed );
			bCmp = CmpGtSIMD( MulSIMD( f4VortexSpeedSign, SubSIMD( f4DestVortexSpeed, f4NewVortexSpeed ) ), Four_Zeros );
			f4CurrentVortexSpeed = MaskedAssign( bCmp, f4NewVortexSpeed, f4DestVortexSpeed );

			// spiral pos around m_vecBeamDir at random speed
			FourVectors v4VortexRadius = v4EndPos - v4VecPointOnline;

			fltx4 f4VortexDirection = { m_data[i].m_flVortexDirection,
										m_data[i+1].m_flVortexDirection,
										m_data[i+2].m_flVortexDirection,
										m_data[i+3].m_flVortexDirection };
			f4VortexDirection = MaskedAssign( bCmpIsBeamReversed, NegSIMD( f4VortexDirection ), f4VortexDirection );


			fltx4 f4AngleOffset = MulSIMD( f4DeltaTime, MulSIMD( f4VortexDirection, f4CurrentVortexSpeed ) );

			// apply rotations
			FourQuaternions q4Rotations;
			q4Rotations.FromAxisAndAnglesInDegrees( f4BeamDir, f4AngleOffset );
			q4Rotations.RotateFourVectors( &v4VortexRadius );
		
			v4VortexRadius.VectorNormalize();
			v4VortexRadius = Mul( v4VortexRadius, f4CurrentVortexRadius );

			FourVectors v4NewPos = v4VecPointOnline + v4VortexRadius;
			// add angular velocity
			FourVectors v4DiffPos = v4NewPos - v4EndPos;
			FourVectors v4AngularVelocity = Mul( v4DiffPos, f4InvDeltaTime );

			// scale angular velocity by how far off the center of the beam
			FourVectors v4DisFromCenterVec = v4VecPointOnline - v4EndPos;
			fltx4 f4DistFromCenterSqr = v4DisFromCenterVec.length2(); // lengthSqr
			fltx4 f4AngularVelocityScale = RemapValClampedSIMD( f4DistFromCenterSqr, Four_Zeros, f4BeamRadiusSqr, Four_Zeros, Four_Ones );

			v4EndVelocity = Mul( v4AngularVelocity, f4AngularVelocityScale ) + v4EndVelocity;

			// output radius
			fltx4 f4FinalRadiusScale = RemapValClampedSIMD( f4NewVortexRadius, Four_Ones, f4BeamRadius, f4MinVortexRadiusScale, Four_Ones );
			m_data[i].m_flCurrentVortexRadius = SubFloat( f4FinalRadiusScale, 0 );
			m_data[i+1].m_flCurrentVortexRadius = SubFloat( f4FinalRadiusScale, 1 );
			m_data[i+2].m_flCurrentVortexRadius = SubFloat( f4FinalRadiusScale, 2 );
			m_data[i+3].m_flCurrentVortexRadius = SubFloat( f4FinalRadiusScale, 3 );

			// output speed
			m_data[i].m_flCurrentVortexSpeed = SubFloat( f4CurrentVortexSpeed, 0 );
			m_data[i+1].m_flCurrentVortexSpeed = SubFloat( f4CurrentVortexSpeed, 1 );
			m_data[i+2].m_flCurrentVortexSpeed = SubFloat( f4CurrentVortexSpeed, 2 );
			m_data[i+3].m_flCurrentVortexSpeed = SubFloat( f4CurrentVortexSpeed, 3 );

			// output velocity
			v4EndVelocity.StoreUnalignedVector3SIMD( &m_data[i].m_vVelocity,
													&m_data[i+1].m_vVelocity,
													&m_data[i+2].m_vVelocity,
													&m_data[i+3].m_vVelocity );

			// output position
			v4NewPos.StoreUnalignedVector3SIMD( &m_data[i].m_vPosition,
												&m_data[i+1].m_vPosition,
												&m_data[i+2].m_vPosition,
												&m_data[i+3].m_vPosition );

#ifdef BLOB_SIMD_DEBUG
			for ( int k=0; k<4; ++k )
			{
				Assert( fabs( m_data[i + k].m_flCurrentVortexRadius - m_data[i + k].m_flCurrentVortexRadius_DEBUG ) < BLOB_IN_BEAM_ERROR );

				Assert( fabs( m_data[i + k].m_flCurrentVortexSpeed - m_data[i + k].m_flCurrentVortexSpeed_DEBUG ) < BLOB_IN_BEAM_ERROR );

				Assert( fabs( m_data[i + k].m_vPosition.x - m_data[i + k].m_vPosition_DEBUG.x ) < BLOB_IN_BEAM_ERROR );
				Assert( fabs( m_data[i + k].m_vPosition.y - m_data[i + k].m_vPosition_DEBUG.y ) < BLOB_IN_BEAM_ERROR );
				Assert( fabs( m_data[i + k].m_vPosition.z - m_data[i + k].m_vPosition_DEBUG.z ) < BLOB_IN_BEAM_ERROR );

				Assert( fabs( m_data[i + k].m_vVelocity.x - m_data[i + k].m_vVelocity_DEBUG.x ) < BLOB_IN_BEAM_ERROR );
				Assert( fabs( m_data[i + k].m_vVelocity.y - m_data[i + k].m_vVelocity_DEBUG.y ) < BLOB_IN_BEAM_ERROR );
				Assert( fabs( m_data[i + k].m_vVelocity.z - m_data[i + k].m_vVelocity_DEBUG.z ) < BLOB_IN_BEAM_ERROR );
			}
#endif

			count -= 4;
			i += 4;
		}

		while ( count > 0 )
		{
			fltx4 f4Speed = Dot3SIMD( LoadUnaligned3SIMD( m_data[i].m_vVelocity.Base() ), f4BeamDir );

			// if speed < 0, set it to 0
			f4Speed = MaskedAssign( CmpLtSIMD( f4Speed, Four_Zeros ), Four_Zeros, f4Speed );

			// ( f4Speed < f4NewSpeed ) ? f4Speed : f4NewSpeed;
			fltx4 f4NewSpeed = AddSIMD( f4Speed, f4BeamAccelDT );
			f4Speed = MaskedAssign( CmpLtSIMD( f4NewSpeed, f4BeamSpeed ), f4NewSpeed, f4BeamSpeed );

			fltx4 f4EndVelocity = MulSIMD( f4Speed, f4BeamDir );
			fltx4 f4EndPos = MaddSIMD( f4DeltaTime, f4EndVelocity, LoadUnaligned3SIMD( m_data[i].m_vPosition.Base() ) );

			// vortex, depending on where the blob is a long the beam
			Vector vecPointOnLine;
			float flDistOnLine;
			Vector vTempEndPos;
			StoreUnaligned3SIMD( vTempEndPos.Base(), f4EndPos );
			CalcClosestPointOnLine( vTempEndPos, vBeamStart, vBeamEnd, vecPointOnLine, &flDistOnLine );
			flDistOnLine *= flBeamLength;

			// compute circulation, if endpos is within the vortex distance, we should vortex at a faster rate (going through portal)
			const float flDistFromBeamCenter = fabs( flDistOnLine - flHalfBeamLength );
			float flCirculation = fsel( flHalfBeamLength - flDistFromBeamCenter - flVortexDistance, flTbeamCirculation, flTbeamPortalCirculation );

			// current vortex radius
			float flDestVortexRadiusScale = ( flBeamLength - flDistOnLine - flVortexDistance < 0.f && bGoingTowardsPortal ) ? flMinVortexRadiusScale : m_data[i].m_flDestVortexRadius;
			float flDestVortexRadius = flDestVortexRadiusScale * flBeamRadius;
			float flCurrentVortexRadiusScale = m_data[i].m_flCurrentVortexRadius;
			float flCurrentVortexRadius = flCurrentVortexRadiusScale * flBeamRadius;

			// current vortex speed
			float flDestVortexSpeed = flCirculation / ( TWO_PI * flCurrentVortexRadius );
			float flCurrentVortexSpeed = m_data[i].m_flCurrentVortexSpeed;

			// compute new vortex radius
			float flSign = Sign( flDestVortexRadius - flCurrentVortexRadius );
			float flNewVortexRadius = flCurrentVortexRadius + flSign * flVortexRadiusOffsetDT;
			flCurrentVortexRadius = fsel( flSign * ( flDestVortexRadius - flNewVortexRadius ), flNewVortexRadius, flDestVortexRadius );

			// compute new vortex speed
			float flVortexSpeedSign = Sign( flDestVortexSpeed - flCurrentVortexSpeed );
			float flNewVortexSpeed = flCurrentVortexSpeed + flVortexSpeedSign * flVortexAccelDT;
			flCurrentVortexSpeed = fsel( flVortexSpeedSign * ( flDestVortexSpeed - flNewVortexSpeed ), flNewVortexSpeed, flDestVortexSpeed );


			// spiral pos around m_vecBeamDir at random speed
			fltx4 f4PointOnLine = LoadUnaligned3SIMD( vecPointOnLine.Base() );
			fltx4 f4VortexRadius = SubSIMD( f4EndPos, f4PointOnLine );
			float flVortexDirection = ( bIsBeamReversed ) ? -m_data[i].m_flVortexDirection : m_data[i].m_flVortexDirection;
			
			Quaternion qRotate;
			AxisAngleQuaternion(vBeamDir, flDeltaTime * flVortexDirection * flCurrentVortexSpeed, qRotate );
			Vector vecVortexRadius;
			StoreUnaligned3SIMD( vecVortexRadius.Base(), f4VortexRadius );
			Vector vecTemp = vecVortexRadius;
			VectorRotate( vecTemp, qRotate, vecVortexRadius );
			f4VortexRadius = LoadUnaligned3SIMD( vecVortexRadius.Base() );

			f4VortexRadius = MulSIMD( ReplicateX4( flCurrentVortexRadius ), Normalized3SIMD( f4VortexRadius ) );

			fltx4 f4NewPos = AddSIMD( f4PointOnLine, f4VortexRadius );
			// add angular velocity
			fltx4 f4DiffPos = SubSIMD( f4NewPos, f4EndPos );
			fltx4 f4AngularVelocity = MulSIMD( f4DiffPos, f4InvDeltaTime );

			// scale angular velocity by how far off the center of the beam
			fltx4 f4DisFromCenterVec = SubSIMD( f4PointOnLine, f4EndPos );
			fltx4 f4DistFromCenterSqr = Dot3SIMD( f4DisFromCenterVec, f4DisFromCenterVec ); // lengthSqr
			fltx4 f4AngularVelocityScale = RemapValClampedSIMD( f4DistFromCenterSqr, Four_Zeros, f4BeamRadiusSqr, Four_Zeros, Four_Ones );

			f4EndVelocity = MaddSIMD( f4AngularVelocityScale, f4AngularVelocity, f4EndVelocity );

			// Output
			float flFinalRadiusScale = RemapValClamped( flCurrentVortexRadius, 1.f, flBeamRadius, flMinVortexRadiusScale, 1.f );
			m_data[i].m_flCurrentVortexRadius = flFinalRadiusScale;
			m_data[i].m_flCurrentVortexSpeed = flCurrentVortexSpeed;
			StoreUnaligned3SIMD( m_data[i].m_vVelocity.Base(), f4EndVelocity );
			StoreUnaligned3SIMD( m_data[i].m_vPosition.Base(), f4NewPos );

#ifdef BLOB_SIMD_DEBUG
			Assert( fabs( m_data[i].m_flCurrentVortexRadius - m_data[i].m_flCurrentVortexRadius_DEBUG ) < BLOB_IN_BEAM_ERROR );
			Assert( fabs( m_data[i].m_flCurrentVortexSpeed - m_data[i].m_flCurrentVortexSpeed_DEBUG ) < BLOB_IN_BEAM_ERROR );

			Assert( fabs( m_data[i].m_vPosition.x - m_data[i].m_vPosition_DEBUG.x ) < BLOB_IN_BEAM_ERROR );
			Assert( fabs( m_data[i].m_vPosition.y - m_data[i].m_vPosition_DEBUG.y ) < BLOB_IN_BEAM_ERROR );
			Assert( fabs( m_data[i].m_vPosition.z - m_data[i].m_vPosition_DEBUG.z ) < BLOB_IN_BEAM_ERROR );

			Assert( fabs( m_data[i].m_vVelocity.x - m_data[i].m_vVelocity_DEBUG.x ) < BLOB_IN_BEAM_ERROR );
			Assert( fabs( m_data[i].m_vVelocity.y - m_data[i].m_vVelocity_DEBUG.y ) < BLOB_IN_BEAM_ERROR );
			Assert( fabs( m_data[i].m_vVelocity.z - m_data[i].m_vVelocity_DEBUG.z ) < BLOB_IN_BEAM_ERROR );
#endif

			--count;
			++i;
		}
	}

	float m_flDeltaTime;
	CTrigger_TractorBeam *m_pBeam;
	CUtlVector< BlobBeamUpdateData_t, CUtlMemoryAligned< BlobBeamUpdateData_t, 16 > > m_data;
};


class BlobsInAirUpdate_SIMD
{
public:
	BlobsInAirUpdate_SIMD( const PaintBlobVector_t& blobs )
	{
		if ( blobs.Count() == 0 )
			return;

		m_flDeltaTime = gpGlobals->curtime - blobs[0]->GetLastUpdateTime();

		int numBlobs = blobs.Count();
		m_data.EnsureCount( numBlobs );
		for ( int i=0; i<numBlobs; ++i )
		{
			m_data[i].m_f4Position = LoadUnaligned3SIMD( blobs[i]->GetPosition().Base() );
			m_data[i].m_f4Velocity = LoadUnaligned3SIMD( blobs[i]->GetVelocity().Base() );
#ifdef BLOB_SIMD_DEBUG
			m_data[i].m_vPosition = blobs[i]->GetPosition();
			m_data[i].m_vVelocity = blobs[i]->GetVelocity();
#endif
		}

		UpdateBlobsInAir_SIMD();

		for ( int i=0; i<numBlobs; ++i )
		{
			Vector pos;
			StoreUnaligned3SIMD( pos.Base(), m_data[i].m_f4Position );
			blobs[i]->SetTempEndPosition( pos );

			Vector vel;
			StoreUnaligned3SIMD( vel.Base(), m_data[i].m_f4Velocity );
			blobs[i]->SetTempEndVelocity( vel );
		}
	}

	~BlobsInAirUpdate_SIMD()
	{
		m_data.Purge();
	}
private:
	struct BlobAirUpdateData_t
	{
		fltx4 m_f4Position;
		fltx4 m_f4Velocity;
#ifdef BLOB_SIMD_DEBUG
		Vector m_vPosition;
		Vector m_vVelocity;
#endif
	};

	void UpdateBlobsInAir_SIMD()
	{
#ifdef BLOB_SIMD_DEBUG
		const float flDeltaTime = m_flDeltaTime;
		float flNewSpeedFraction = ( 1.f - paintblob_air_drag.GetFloat() * flDeltaTime );

		for ( int j=0; j<m_data.Count(); ++j )
		{
			Vector vecMove;

			Vector vecAbsVelocity = m_data[j].m_vVelocity;

			vecMove.x = vecAbsVelocity.x * flDeltaTime;
			vecMove.y = vecAbsVelocity.y * flDeltaTime;

			//Apply gravity
			float newZVelocity = vecAbsVelocity.z - paintblob_gravity_scale.GetFloat() * sv_gravity.GetFloat() * flDeltaTime;
			vecMove.z = 0.5f * ( vecAbsVelocity.z + newZVelocity ) * flDeltaTime;

			vecAbsVelocity.z = newZVelocity;

			//Apply air drag
			m_data[j].m_vVelocity = vecAbsVelocity * flNewSpeedFraction;
			m_data[j].m_vPosition = m_data[j].m_vPosition + vecMove;
		}
#endif

		fltx4 f4DeltaTime = ReplicateX4( m_flDeltaTime );
		fltx4 f4Gravity = { 0.f, 0.f, paintblob_gravity_scale.GetFloat() * sv_gravity.GetFloat(), 0.f };
		fltx4 f4GravityDT = MulSIMD( f4Gravity, f4DeltaTime );
		fltx4 f4NewSpeedFraction = MsubSIMD( ReplicateX4( paintblob_air_drag.GetFloat() ), f4DeltaTime, Four_Ones );
		fltx4 f4HalfDeltaTime = MulSIMD( Four_PointFives, f4DeltaTime );

		int count = m_data.Count();
		int i = 0;
		while ( count >= 4 )
		{
			fltx4 f4Vel0 = m_data[i].m_f4Velocity;
			fltx4 f4Vel1 = m_data[i+1].m_f4Velocity;
			fltx4 f4Vel2 = m_data[i+2].m_f4Velocity;
			fltx4 f4Vel3 = m_data[i+3].m_f4Velocity;

			fltx4 f4NewVel0 = SubSIMD( f4Vel0, f4GravityDT );
			fltx4 f4NewVel1 = SubSIMD( f4Vel1, f4GravityDT );
			fltx4 f4NewVel2 = SubSIMD( f4Vel2, f4GravityDT );
			fltx4 f4NewVel3 = SubSIMD( f4Vel3, f4GravityDT );

			m_data[i].m_f4Velocity = MulSIMD( f4NewVel0, f4NewSpeedFraction );
			m_data[i+1].m_f4Velocity = MulSIMD( f4NewVel1, f4NewSpeedFraction );
			m_data[i+2].m_f4Velocity = MulSIMD( f4NewVel2, f4NewSpeedFraction );
			m_data[i+3].m_f4Velocity = MulSIMD( f4NewVel3, f4NewSpeedFraction );

			m_data[i].m_f4Position = MaddSIMD( AddSIMD( f4Vel0, f4NewVel0 ), f4HalfDeltaTime, m_data[i].m_f4Position );
			m_data[i+1].m_f4Position = MaddSIMD( AddSIMD( f4Vel1, f4NewVel1 ), f4HalfDeltaTime, m_data[i+1].m_f4Position );
			m_data[i+2].m_f4Position = MaddSIMD( AddSIMD( f4Vel2, f4NewVel2 ), f4HalfDeltaTime, m_data[i+2].m_f4Position );
			m_data[i+3].m_f4Position = MaddSIMD( AddSIMD( f4Vel3, f4NewVel3 ), f4HalfDeltaTime, m_data[i+3].m_f4Position );

#ifdef BLOB_SIMD_DEBUG
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 0 ) - m_data[i].m_vVelocity.x ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 1 ) - m_data[i].m_vVelocity.y ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 2 ) - m_data[i].m_vVelocity.z ) < 0.001f );

			Assert( fabs( SubFloat( m_data[i].m_f4Position, 0 ) - m_data[i].m_vPosition.x ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Position, 1 ) - m_data[i].m_vPosition.y ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Position, 2 ) - m_data[i].m_vPosition.z ) < 0.001f );
#endif

			i += 4;
			count -= 4;
		}

		// do the rest
		while ( count > 0 )
		{
			fltx4 f4Vel = m_data[i].m_f4Velocity;
			fltx4 f4NewVel = SubSIMD( f4Vel, f4GravityDT );

			m_data[i].m_f4Velocity = MulSIMD( f4NewVel, f4NewSpeedFraction );
			m_data[i].m_f4Position = MaddSIMD( AddSIMD( f4Vel, f4NewVel ), f4HalfDeltaTime, m_data[i].m_f4Position );

#ifdef BLOB_SIMD_DEBUG
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 0 ) - m_data[i].m_vVelocity.x ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 1 ) - m_data[i].m_vVelocity.y ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 2 ) - m_data[i].m_vVelocity.z ) < 0.001f );

			Assert( fabs( SubFloat( m_data[i].m_f4Position, 0 ) - m_data[i].m_vPosition.x ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Position, 1 ) - m_data[i].m_vPosition.y ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Position, 2 ) - m_data[i].m_vPosition.z ) < 0.001f );
#endif
			++i;
			--count;
		}
	}

	float m_flDeltaTime;
	CUtlVector< BlobAirUpdateData_t, CUtlMemoryAligned< BlobAirUpdateData_t, 16 > > m_data;
};

class BlobsInStreakUpdate_SIMD
{
public:
	BlobsInStreakUpdate_SIMD( const PaintBlobVector_t& blobs )
	{
		if ( blobs.Count() == 0 )
			return;

		m_flDeltaTime = gpGlobals->curtime - blobs[0]->GetLastUpdateTime();

		int numBlobs = blobs.Count();
		m_data.EnsureCount( numBlobs );
		for ( int i=0; i<numBlobs; ++i )
		{
			m_data[i].m_f4Position = LoadUnaligned3SIMD( blobs[i]->GetPosition().Base() );
			m_data[i].m_f4Velocity = LoadUnaligned3SIMD( blobs[i]->GetVelocity().Base() );
			m_data[i].m_f4StreakDir = LoadUnaligned3SIMD( blobs[i]->GetStreakDir().Base() );

#ifdef BLOB_SIMD_DEBUG
			m_data[i].m_vPosition = blobs[i]->GetPosition();
			m_data[i].m_vVelocity = blobs[i]->GetVelocity();
			m_data[i].m_vStreakDir = blobs[i]->GetStreakDir();
#endif
		}

		UpdateBlobsInStreak_SIMD();

		for ( int i=0; i<numBlobs; ++i )
		{
			Vector pos;
			StoreUnaligned3SIMD( pos.Base(), m_data[i].m_f4Position );
			blobs[i]->SetTempEndPosition( pos );

			Vector vel;
			StoreUnaligned3SIMD( vel.Base(), m_data[i].m_f4Velocity );
			blobs[i]->SetTempEndVelocity( vel );
		}
	}

	~BlobsInStreakUpdate_SIMD()
	{
		m_data.Purge();
	}

private:
	struct BlobStreakUpdateData_t
	{
		fltx4 m_f4Position;
		fltx4 m_f4Velocity;
		fltx4 m_f4StreakDir;

#ifdef BLOB_SIMD_DEBUG
		Vector m_vPosition;
		Vector m_vVelocity;
		Vector m_vStreakDir;
#endif
	};

	void UpdateBlobsInStreak_SIMD()
	{
		fltx4 f4DeltaTime = ReplicateX4( m_flDeltaTime );
		fltx4 f4Gravity = { 0.f, 0.f, paintblob_gravity_scale.GetFloat() * sv_gravity.GetFloat(), 0.f };
		fltx4 f4GravityDT = MulSIMD( f4Gravity, f4DeltaTime );
		fltx4 f4HalfDeltaTime = MulSIMD( Four_PointFives, f4DeltaTime );

#ifdef BLOB_SIMD_DEBUG
		const float flDeltaTime = m_flDeltaTime;
		float flGravity = paintblob_gravity_scale.GetFloat() * sv_gravity.GetFloat() * flDeltaTime;
#endif

		for ( int i=0; i<m_data.Count(); ++i )
		{
#ifdef BLOB_SIMD_DEBUG
			Vector vecMove;
			Vector vecVelocity = m_data[i].m_vVelocity;

			Vector vecAbsVelocity = vecVelocity;
			vecMove.x = vecAbsVelocity.x * flDeltaTime;
			vecMove.y = vecAbsVelocity.y * flDeltaTime;

			//Apply gravity
			float newZVelocity = vecAbsVelocity.z - flGravity;
			vecMove.z = 0.5f * ( vecAbsVelocity.z + newZVelocity ) * flDeltaTime;
			vecAbsVelocity.z = newZVelocity;

			//Clip the velocity to the streak surface if the blob is streaking
			Vector vecSurfaceNormal = -m_data[i].m_vStreakDir;
			m_data[i].m_vVelocity = vecVelocity - DotProduct( vecSurfaceNormal, vecVelocity ) * vecSurfaceNormal;
			m_data[i].m_vPosition = ( vecMove - DotProduct( vecSurfaceNormal, vecMove ) * vecSurfaceNormal ) + m_data[i].m_vPosition;
#endif

			// SIMD
			fltx4 f4Vel = m_data[i].m_f4Velocity;
			fltx4 f4NewVel = SubSIMD( f4Vel, f4GravityDT );
			fltx4 f4Move = MulSIMD( AddSIMD( f4Vel, f4NewVel ), f4HalfDeltaTime );

			fltx4 f4SurfaceNormal = m_data[i].m_f4StreakDir;
			m_data[i].m_f4Velocity = SubSIMD( f4Vel, MulSIMD( Dot3SIMD( f4SurfaceNormal, f4Vel ), f4SurfaceNormal ) );
			m_data[i].m_f4Position = AddSIMD( MsubSIMD( Dot3SIMD( f4SurfaceNormal, f4Move), f4SurfaceNormal, f4Move ), m_data[i].m_f4Position );

#ifdef BLOB_SIMD_DEBUG
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 0 ) - m_data[i].m_vVelocity.x ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 1 ) - m_data[i].m_vVelocity.y ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Velocity, 2 ) - m_data[i].m_vVelocity.z ) < 0.001f );

			Assert( fabs( SubFloat( m_data[i].m_f4Position, 0 ) - m_data[i].m_vPosition.x ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Position, 1 ) - m_data[i].m_vPosition.y ) < 0.001f );
			Assert( fabs( SubFloat( m_data[i].m_f4Position, 2 ) - m_data[i].m_vPosition.z ) < 0.001f );
#endif
		}
	}

	float m_flDeltaTime;
	CUtlVector< BlobStreakUpdateData_t, CUtlMemoryAligned< BlobStreakUpdateData_t, 16 > > m_data;
};


void PaintBlobUpdate( const PaintBlobVector_t& blobList )
{
	if ( blobList.Count() == 0 )
		return;

	PaintBlobVector_t firstPass;
	firstPass.EnsureCount( blobList.Count() );
	int nFirstPassCount = 0;
	for ( int i=0; i<blobList.Count(); ++i )
	{
		CPaintBlob *pBlob = blobList[i];
		Assert( !pBlob->ShouldDeleteThis() );

		float updateDeltaTime = gpGlobals->curtime - pBlob->GetLastUpdateTime();
		if ( updateDeltaTime < 0.001f )
		{
			continue;
		}

		//Update the lifetime of the blob
		pBlob->UpdateLifeTime( updateDeltaTime );

		//If the paint blobs have a limited range
		if( paintblob_limited_range.GetBool() )
		{
			if( pBlob->GetLifeTime() >= paintblob_lifetime.GetFloat() )
			{
				pBlob->SetDeletionFlag( true );
				continue;
			}
		}

		firstPass[ nFirstPassCount ] = pBlob;
		++nFirstPassCount;
	}
	firstPass.RemoveMultipleFromTail( blobList.Count() - nFirstPassCount );

	if ( nFirstPassCount == 0 )
		return;

	PaintBlobVector_t blobsInBeam;
	PaintBlobVector_t blobsInAir;
	PaintBlobVector_t blobsInStreak;
	SplitBlobsIntoMovementGroup( firstPass, blobsInBeam, blobsInAir, blobsInStreak );
	// do SIMD update here
	BlobsInAirUpdate_SIMD blobAirSIMD( blobsInAir );
	BlobsInStreakUpdate_SIMD blobStreakSIMD( blobsInStreak );


	int nSecondPassCount = 0;
	PaintBlobVector_t secondPass;
	secondPass.EnsureCount( firstPass.Count() );

	V_memcpy( secondPass.Base(), blobsInAir.Base(), blobsInAir.Count() * sizeof( CBasePaintBlob* ) );
	nSecondPassCount += blobsInAir.Count();
	V_memcpy( secondPass.Base() + nSecondPassCount, blobsInStreak.Base(), blobsInStreak.Count() * sizeof( CBasePaintBlob* ) );
	nSecondPassCount += blobsInStreak.Count();


	int totalBlobsInBeams = 0;
	for( int i = 0; i < ITriggerTractorBeamAutoList::AutoList().Count(); ++i )
	{
		CTrigger_TractorBeam *pBeam = static_cast< CTrigger_TractorBeam* >( ITriggerTractorBeamAutoList::AutoList()[i] );
		int numBlobsInBeam = pBeam->m_blobs.Count();

		if ( numBlobsInBeam == 0 )
			continue;

		// update tempEndPos, tempEndVel
		BlobsInBeamUpdate_SIMD blobBeamSIMD( pBeam );

		totalBlobsInBeams += numBlobsInBeam;

		const Vector& vecBeamStart = pBeam->GetStartPoint();
		const Vector& vecBeamEnd = pBeam->GetEndPoint();
		float flBeamRadius = pBeam->GetBeamRadius();

		// TODO: trace beam ray, if not hit anything, skip all these blobs
		Ray_t blobRay;
		Vector vBeamExtents( 0.f, flBeamRadius, flBeamRadius );
		blobRay.Init( vecBeamStart, vecBeamEnd, -vBeamExtents, vBeamExtents );

		CBaseEntity* ppEntities[ MAX_BLOB_TRACE_ENTITY_RESULTS ];
		BlobTraceEnum entEnum( ppEntities, ARRAYSIZE( ppEntities ), BLOB_TRACE_DYNAMIC_MASK );
		UTIL_Blob_EnumerateEntitiesAlongRay( blobRay, &entEnum );

		CUtlVector< std::pair< float, float > > beamBadSectionList;
		Ray_t invBlobRay;
		invBlobRay.Init( vecBeamEnd, vecBeamStart, -vBeamExtents, vBeamExtents );
		for ( int i=0; i<entEnum.GetCount(); ++i )
		{
			CBaseEntity *pHitEntity = ppEntities[i];

			if ( pHitEntity == pBeam || FClassnameIs( pHitEntity, "physicsclonearea" ) )
			{
				continue;
			}

			// trace from both sides of the beam to find bad section
			trace_t tr;
			enginetrace->ClipRayToEntity( blobRay, BLOB_TRACE_DYNAMIC_MASK, pHitEntity, &tr );
			trace_t trInv;
			enginetrace->ClipRayToEntity( invBlobRay, BLOB_TRACE_DYNAMIC_MASK, pHitEntity, &trInv );

			std::pair< float, float > badSection;
			badSection.first = fsel( tr.fraction - 1.f, 1.f, tr.fraction );
			badSection.second = fsel( trInv.fraction - 1.f, 0.f, 1.f - trInv.fraction );

			// if assert fail, means we missed the entity
			//Assert( badSection.first < badSection.second );
			if ( badSection.first >= badSection.second )
			{
				//DevMsg("bad entity: %s\n", pHitEntity->GetClassname() );
				continue;
			}
			/*else
			{
				DevMsg("time: %f, name: %s, range: [%f, %f]\n", gpGlobals->curtime, pHitEntity->GetClassname(), badSection.first, badSection.second );
			}*/

			beamBadSectionList.AddToTail( badSection );
		}

		if ( debug_beam_badsection.GetBool() )
		{
			Vector vecDir = vecBeamEnd - vecBeamStart;
			
			for ( int i=0; i<beamBadSectionList.Count(); ++i )
			{
				const std::pair< float, float >& badSection = beamBadSectionList[i];
				Vector vStart = vecBeamStart + badSection.first * vecDir;
				Vector vEnd = vecBeamStart + badSection.second * vecDir;
				NDebugOverlay::Line( vStart, vEnd, 255, 255, 0, true, 0.1f );
			}
		}
		
		// check where each blob is along the beam and see if they need to go through second pass
		for( int j = 0; j < numBlobsInBeam; ++j )
		{
			CPaintBlob *pBlob = pBeam->m_blobs[j];
			const Vector& vEndPos = pBlob->GetTempEndPosition();

			Vector vClosestPointOnLine;
			float flFractionOnLine;
			CalcClosestPointOnLine( vEndPos, vecBeamStart, vecBeamEnd, vClosestPointOnLine, &flFractionOnLine );

			if ( flFractionOnLine < 0.f || flFractionOnLine > 1.f )
			{
				secondPass[nSecondPassCount] = pBlob;
				++nSecondPassCount;
				continue;
			}

			bool bIsInBadSection = false;
			for ( int k=0; k<beamBadSectionList.Count(); ++k )
			{
				const std::pair< float, float >& badSection = beamBadSectionList[k];
				if ( flFractionOnLine >= badSection.first && flFractionOnLine <= badSection.second )
				{
					bIsInBadSection = true;
					secondPass[nSecondPassCount] = pBlob;
					++nSecondPassCount;
					break;
				}
			}

			if ( bIsInBadSection )
				continue;

			pBlob->SetPosition( pBlob->GetTempEndPosition() );
			pBlob->SetVelocity( pBlob->GetTempEndVelocity() );
			pBlob->ResetGhostState();
		}
	} // end for this beam

	AssertMsg( totalBlobsInBeams == blobsInBeam.Count(), "Blobs are in bad beam state\n");


	// do collision
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	for ( int i=0; i<nSecondPassCount; ++i )
	{
		CBasePaintBlob *pBlob = secondPass[i];

		if ( pBlob->ShouldDeleteThis() )
			continue;

		float updateDeltaTime = gpGlobals->curtime - pBlob->GetLastUpdateTime();
		const Vector& vecEndPos = pBlob->GetTempEndPosition();
		Vector vecEndVelocity = pBlob->GetTempEndVelocity();

		// Exit early if the blob isn't moving
		if( pBlob->GetPosition() == vecEndPos )
		{
			continue;
		}

		pBlob->UpdateBlobCollision( updateDeltaTime, vecEndPos, vecEndVelocity );
		if ( pBlob->ShouldDeleteThis() )
		{
			continue;
		}

		pBlob->UpdateBlobPostCollision( updateDeltaTime );
	}


	// update time for blobs that get in first pass
	for ( int i=0; i<nFirstPassCount; ++i )
	{
		CBasePaintBlob *pBlob = firstPass[i];

		pBlob->SetLastUpdateTime( gpGlobals->curtime );
	}
}

void CBasePaintBlob::SetShouldPlaySound( bool shouldPlaySound )
{
	m_bShouldPlaySound = shouldPlaySound;
}


bool CBasePaintBlob::ShouldPlaySound() const
{
	return m_bShouldPlaySound && !m_bDrawOnly;
}


bool CBasePaintBlob::ShouldPlayEffect() const
{
	return m_bShouldPlayEffect && !m_bDrawOnly;
}
