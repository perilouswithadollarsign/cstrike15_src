//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_UTIL_SHARED_H
#define PORTAL_UTIL_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "engine/IEngineTrace.h"
#include "paint_color_manager.h"

extern bool g_bBulletPortalTrace;

#ifdef CLIENT_DLL
	#include "client_class.h"
	#include "tier1/interpolatedvar.h"
	class CPortalRenderable_FlatBasic;
	class C_Portal_Base2D;
	#define CPortal_Base2D C_Portal_Base2D
	class C_BasePlayer;
	typedef C_BasePlayer CBasePlayer;
#else
	class CPortal_Base2D;
	class CBasePlayer;
#endif

//When tracing through portals, a line becomes a discontinuous collection of segments as it travels
struct ComplexPortalTrace_t
{
	CPortal_Base2D *pSegmentStartPortal;
	CPortal_Base2D *pSegmentEndPortal;
	Vector vNormalizedDelta;
	trace_t trSegment;
};

#if defined ( CLIENT_DLL )
Color UTIL_Portal_Color( int iPortal, int iTeamNumber = 0 );
Color UTIL_Portal_Color_Particles( int iPortal, int iTeamNumber = 0 );
#endif

void UTIL_Portal_Trace_Filter( class CTraceFilterSimpleClassnameList *traceFilterPortalShot );

CPortal_Base2D* UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fMustBeCloserThan, CPortal_Base2D **pSearchArray, int iSearchArrayCount );
CPortal_Base2D* UTIL_Portal_FirstAlongRay( const Ray_t &ray, float &fMustBeCloserThan );

bool UTIL_Portal_TraceRay_Bullets( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true );
CPortal_Base2D* UTIL_Portal_TraceRay_Beam( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, float *pfFraction );

void UTIL_Portal_TraceRay_With( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true );
CPortal_Base2D* UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true ); //traces a ray normally, then sees if portals have anything to say about it
CPortal_Base2D* UTIL_Portal_TraceRay( const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall = true );

void UTIL_Portal_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true ); //traces against a specific portal's environment, does no *real* tracing
void UTIL_Portal_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall = true );

void UTIL_PortalLinked_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace, bool bTraceHolyWall = true ); //traces against a specific portal's environment, does no *real* tracing
void UTIL_PortalLinked_TraceRay( const CPortal_Base2D *pPortal, const Ray_t &ray, unsigned int fMask, const IHandleEntity *ignore, int collisionGroup, trace_t *pTrace, bool bTraceHolyWall = true );

abstract_class ICountedPartitionEnumerator : public IPartitionEnumerator
{
public:
	virtual int GetCount() const = 0;
};

int UTIL_Portal_EntitiesAlongRayComplex( int *entSegmentIndices, int *segCount, int maxEntities, ComplexPortalTrace_t *pResultSegmentArray, int maxSegments, const Ray_t& ray, ICountedPartitionEnumerator* pEnum, ITraceFilter* pTraceFilter, int fStopTraceContents );

// tests if a ray's trace hits any portals
bool UTIL_DidTraceTouchPortals ( const Ray_t& ray, const trace_t& trace, CPortal_Base2D** pOutLocal = NULL, CPortal_Base2D** pOutRemote = NULL );

// Version of the TraceEntity functions which trace through portals
void UTIL_Portal_TraceEntity( CBaseEntity *pEntity, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
							 unsigned int mask, ITraceFilter *pFilter, trace_t *ptr );

//Starts off as a normal trace, but as it hits portals it adds segments up to the limit. Returns number of segments used. Assumes a ray only travels through a portal if the ray's center hits the quad
int UTIL_Portal_ComplexTraceRay( const Ray_t &ray, unsigned int mask, ITraceFilter *pTraceFilter, ComplexPortalTrace_t *pResultSegmentArray, int iMaxSegments );

void UTIL_Portal_PointTransform( const VMatrix &matThisToLinked, const Vector &ptSource, Vector &ptTransformed );
void UTIL_Portal_VectorTransform( const VMatrix &matThisToLinked, const Vector &vSource, Vector &vTransformed );
void UTIL_Portal_AngleTransform( const VMatrix &matThisToLinked, const QAngle &qSource, QAngle &qTransformed );
void UTIL_Portal_RayTransform( const VMatrix &matThisToLinked, const Ray_t &raySource, Ray_t &rayTransformed );
void UTIL_Portal_PlaneTransform( const VMatrix &matThisToLinked, const cplane_t &planeSource, cplane_t &planeTransformed );
void UTIL_Portal_PlaneTransform( const VMatrix &matThisToLinked, const VPlane &planeSource, VPlane &planeTransformed );

void UTIL_Portal_Triangles( const Vector &ptPortalCenter, const QAngle &qPortalAngles, float fHalfWidth, float fHalfHeight, Vector pvTri1[ 3 ], Vector pvTri2[ 3 ] );
void UTIL_Portal_Triangles( const CPortal_Base2D *pPortal, Vector pvTri1[ 3 ], Vector pvTri2[ 3 ] );
void UTIL_Portal_AABB( const CPortal_Base2D *pPortal, Vector &vMin, Vector &vMax );

float UTIL_Portal_DistanceThroughPortal( const CPortal_Base2D *pPortal, const Vector &vPoint1, const Vector &vPoint2 );
float UTIL_Portal_DistanceThroughPortalSqr( const CPortal_Base2D *pPortal, const Vector &vPoint1, const Vector &vPoint2 );
float UTIL_Portal_ShortestDistance( const Vector &vPoint1, const Vector &vPoint2, CPortal_Base2D **pShortestDistPortal_Out = NULL, bool bRequireStraightLine = false );
float UTIL_Portal_ShortestDistanceSqr( const Vector &vPoint1, const Vector &vPoint2, CPortal_Base2D **pShortestDistPortal_Out = NULL, bool bRequireStraightLine = false );

void UTIL_Portal_VectorToGlobalTransforms( const Vector &vPoint2, CUtlVector< Vector > *utlVecPositions );

//-----------------------------------------------------------------------------
//
// UTIL_IntersectRayWithPortal
//
// Intersects a ray with a portal, returns distance t along ray.
// t will be less than zero if no intersection occurred
//
//-----------------------------------------------------------------------------
float UTIL_IntersectRayWithPortal( const Ray_t &ray, const CPortal_Base2D *pPortal );

bool UTIL_IntersectRayWithPortalOBB( const CPortal_Base2D *pPortal, const Ray_t &ray, trace_t *pTrace );
bool UTIL_IntersectRayWithPortalOBBAsAABB( const CPortal_Base2D *pPortal, const Ray_t &ray, trace_t *pTrace );

bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const Vector &ptPortalCenter, const QAngle &qPortalAngles, float fPortalHalfWidth, float fPortalHalfHeight, float flTolerance = 0.0f );
bool UTIL_IsBoxIntersectingPortal( const Vector &vecBoxCenter, const Vector &vecBoxExtents, const CPortal_Base2D *pPortal, float flTolerance = 0.0f );

CPortal_Base2D *UTIL_IntersectEntityExtentsWithPortal( const CBaseEntity *pEntity );

void UTIL_Portal_NDebugOverlay( const Vector &ptPortalCenter, const QAngle &qPortalAngles, float fHalfWidth, float fHalfHeight, int r, int g, int b, int a, bool noDepthTest, float duration );
void UTIL_Portal_NDebugOverlay( const CPortal_Base2D *pPortal, int r, int g, int b, int a, bool noDepthTest, float duration );

bool UTIL_FindClosestPassableSpace_InPortal( const CPortal_Base2D *pPortal, const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut );
bool UTIL_FindClosestPassableSpace_InPortal_CenterMustStayInFront( const CPortal_Base2D *pPortal, const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut );
bool FindClosestPassableSpace( CBaseEntity *pEntity, const Vector &vIndecisivePush, unsigned int fMask = MASK_SOLID ); //assumes the object is already in a mostly passable space
bool UTIL_FindClosestPassableSpace_CenterMustStayInFrontOfPlane( const Vector &vCenter, const Vector &vExtents, const Vector &vIndecisivePush, ITraceFilter *pTraceFilter, unsigned int fMask, unsigned int iIterations, Vector &vCenterOut, const VPlane &stayInFrontOfPlane );

CPortal_Base2D *UTIL_PointIsOnPortalQuad( const Vector vPoint, float fOnPlaneEpsilon, CPortal_Base2D * const *pPortalsToCheck, int iArraySize );

#ifdef CLIENT_DLL
void UTIL_TransformInterpolatedAngle( CInterpolatedVar< QAngle > &qInterped, matrix3x4_t matTransform, float fUpToTime );
void UTIL_TransformInterpolatedPosition( CInterpolatedVar< Vector > &vInterped, const VMatrix& matTransform, float fUpToTime );
#endif

bool UTIL_Portal_EntityIsInPortalHole( const CPortal_Base2D *pPortal, const CBaseEntity *pEntity );

extern const Vector UTIL_ProjectPointOntoPlane( const Vector& point, const cplane_t& plane );
bool UTIL_PointIsNearPortal( const Vector& point, const CPortal_Base2D* pPortal2D, float planeDist, float radiusReduction = 0.0f );

bool UTIL_IsEntityMovingOrRotating( CBaseEntity* pEntity );


// mainly use for stick camera
void UTIL_NormalizedAngleDiff( const QAngle& start, const QAngle& end, QAngle* result );

#if defined( CLIENT_DLL )
void UTIL_Portal_ComputeMatrix( CPortalRenderable_FlatBasic *pLocalPortal, CPortalRenderable_FlatBasic *pRemotePortal );
#else
void UTIL_Portal_ComputeMatrix( CPortal_Base2D *pLocalPortal, CPortal_Base2D *pRemotePortal );
#endif


// PAINT
bool UTIL_IsPaintableSurface( const csurface_t& surface );

float UTIL_PaintBrushEntity( CBaseEntity* pBrushEntity, const Vector& contactPoint, PaintPowerType power, float flPaintRadius, float flAlphaPercent );
PaintPowerType UTIL_Paint_TracePower( CBaseEntity* pBrushEntity, const Vector& contactPoint, const Vector& vContactNormal );

// output start point and reflect dir
bool UTIL_Paint_Reflect( const trace_t& tr, Vector& vStart, Vector& vDir, PaintPowerType reflectPower = REFLECT_POWER );



//To extend radius's through portals (for explosions, etc.)
struct PortalRadiusExtension_t
{
	CPortal_Base2D *pPortalFrom;
	CPortal_Base2D *pPortalTo;
	Vector vecOrigin;
	QAngle vecAngles;
};
typedef CUtlVector<PortalRadiusExtension_t> PortalRadiusExtensionVector;
void ExtendRadiusThroughPortals( const Vector &vecOrigin, const QAngle &vecAngles, float flRadius, PortalRadiusExtensionVector &portalRadiusExtensions );

void UTIL_Portal_Laser_Prevent_Tilting( Vector& vDirection );

class CPolyhedron;
class CPhysCollide;
void UTIL_DebugOverlay_Polyhedron( const CPolyhedron *pPolyhedron, int red, int green, int blue, bool noDepthTest, float flDuration, const matrix3x4_t *pTransform = NULL );
void UTIL_DebugOverlay_CPhysCollide( const CPhysCollide *pCollide, int red, int green, int blue, bool noDepthTest, float flDuration, const matrix3x4_t *pTransform = NULL );

bool UTIL_IsCollideableIntersectingPhysCollide( ICollideable *pCollideable, const CPhysCollide *pCollide, const Vector &vPhysCollideOrigin, const QAngle &qPhysCollideAngles );

CBasePlayer* UTIL_OtherPlayer( CBasePlayer const* pPlayer );

#ifdef GAME_DLL
CBasePlayer* UTIL_OtherConnectedPlayer( CBasePlayer const* pPlayer );
#endif

#ifdef GAME_DLL

class CBrushEntityList : public IEntityEnumerator
{
public:
	virtual bool EnumEntity( IHandleEntity *pHandleEntity );

	CUtlVectorFixedGrowable< CBaseEntity*, 32 > m_BrushEntitiesToPaint;
};

void UTIL_FindBrushEntitiesInSphere( CBrushEntityList& brushEnum, const Vector& vCenter, float flRadius );

#endif

#endif //#ifndef PORTAL_UTIL_SHARED_H
