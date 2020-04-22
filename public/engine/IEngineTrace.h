//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef ENGINE_IENGINETRACE_H
#define ENGINE_IENGINETRACE_H
#ifdef _WIN32
#pragma once
#endif

#include "basehandle.h"
#include "utlvector.h" //need CUtlVector for IEngineTrace::GetBrushesIn*()
#include "mathlib/vector4d.h"
#include "bspflags.h"

class Vector;
class IHandleEntity;
struct Ray_t;
class CGameTrace;
typedef CGameTrace trace_t;
class ICollideable;
class QAngle;
class ITraceListData;
class CPhysCollide;
struct cplane_t;
struct virtualmeshlist_t;
struct AABB_t;

//-----------------------------------------------------------------------------
// The standard trace filter... NOTE: Most normal traces inherit from CTraceFilter!!!
//-----------------------------------------------------------------------------
enum TraceType_t
{
	TRACE_EVERYTHING = 0,
	TRACE_WORLD_ONLY,				// NOTE: This does *not* test static props!!!
	TRACE_ENTITIES_ONLY,			// NOTE: This version will *not* test static props
	TRACE_EVERYTHING_FILTER_PROPS,	// NOTE: This version will pass the IHandleEntity for props through the filter, unlike all other filters
};

abstract_class ITraceFilter
{
public:
	virtual bool ShouldHitEntity( IHandleEntity *pEntity, int contentsMask ) = 0;
	virtual TraceType_t	GetTraceType() const = 0;
};

struct OcclusionTestResults_t
{
	VectorAligned vEndMin, vEndMax; // the bounding box enclosing the end of the occlusion test, to be used to extend the bounding box of an object shadow 
};

//-----------------------------------------------------------------------------
// Classes are expected to inherit these + implement the ShouldHitEntity method
//-----------------------------------------------------------------------------

// This is the one most normal traces will inherit from
class CTraceFilter : public ITraceFilter
{
public:
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING;
	}
};

class CTraceFilterEntitiesOnly : public ITraceFilter
{
public:
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}
};


//-----------------------------------------------------------------------------
// Classes need not inherit from these
//-----------------------------------------------------------------------------
class CTraceFilterWorldOnly : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_WORLD_ONLY;
	}
};

class CTraceFilterWorldAndPropsOnly : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		return false;
	}
	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING;
	}
};

class CTraceFilterHitAll : public CTraceFilter
{
public:
	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{ 
		return true; 
	}
};


enum DebugTraceCounterBehavior_t
{
	kTRACE_COUNTER_SET = 0,
	kTRACE_COUNTER_INC,
};

//-----------------------------------------------------------------------------
// Enumeration interface for EnumerateLinkEntities
//-----------------------------------------------------------------------------
abstract_class IEntityEnumerator
{
public:
	// This gets called with each handle
	virtual bool EnumEntity( IHandleEntity *pHandleEntity ) = 0; 
};


struct BrushSideInfo_t
{
	cplane_t plane;			// The plane of the brush side
	unsigned short bevel;	// Bevel plane?
	unsigned short thin;	// Thin?
};

//something we can easily create on the stack while easily maintaining our data release contract with IEngineTrace
class CBrushQuery
{
public:
	CBrushQuery( void )
	{
		m_iCount = 0;
		m_pBrushes = NULL;
		m_iMaxBrushSides = 0;
		m_pReleaseFunc = NULL;
		m_pData = NULL;
	}		
	~CBrushQuery( void )
	{
		ReleasePrivateData();
	}
	void ReleasePrivateData( void )
	{
		if( m_pReleaseFunc )
		{
			m_pReleaseFunc( this );
		}

		m_iCount = 0;
		m_pBrushes = NULL;
		m_iMaxBrushSides = 0;
		m_pReleaseFunc = NULL;
		m_pData = NULL;
	}

	inline int Count( void ) const { return m_iCount; }
	inline uint32 *Base( void ) { return m_pBrushes; }
	inline uint32 operator[]( int iIndex ) const { return m_pBrushes[iIndex]; }	
	inline uint32 GetBrushNumber( int iIndex ) const { return m_pBrushes[iIndex]; }	

	//maximum number of sides of any 1 brush in the query results
	inline int MaxBrushSides( void ) const { return m_iMaxBrushSides; }

protected:
	int m_iCount;
	uint32 *m_pBrushes;
	int m_iMaxBrushSides; 
	void (*m_pReleaseFunc)(CBrushQuery *); //release function is almost always in a different dll than calling code
	void *m_pData;
};

//-----------------------------------------------------------------------------
// Interface the engine exposes to the game DLL
//-----------------------------------------------------------------------------
#define INTERFACEVERSION_ENGINETRACE_SERVER	"EngineTraceServer004"
#define INTERFACEVERSION_ENGINETRACE_CLIENT	"EngineTraceClient004"
abstract_class IEngineTrace
{
public:
	// Returns the contents mask + entity at a particular world-space position
	virtual int		GetPointContents( const Vector &vecAbsPosition, int contentsMask = MASK_ALL, IHandleEntity** ppEntity = NULL ) = 0;
	
	// Returns the contents mask of the world only @ the world-space position (static props are ignored)
	virtual int		GetPointContents_WorldOnly( const Vector &vecAbsPosition, int contentsMask = MASK_ALL ) = 0;

	// Get the point contents, but only test the specific entity. This works
	// on static props and brush models.
	//
	// If the entity isn't a static prop or a brush model, it returns CONTENTS_EMPTY and sets
	// bFailed to true if bFailed is non-null.
	virtual int		GetPointContents_Collideable( ICollideable *pCollide, const Vector &vecAbsPosition ) = 0;

	// Traces a ray against a particular entity
	virtual void	ClipRayToEntity( const Ray_t &ray, unsigned int fMask, IHandleEntity *pEnt, trace_t *pTrace ) = 0;

	// Traces a ray against a particular entity
	virtual void	ClipRayToCollideable( const Ray_t &ray, unsigned int fMask, ICollideable *pCollide, trace_t *pTrace ) = 0;

	// A version that simply accepts a ray (can work as a traceline or tracehull)
	virtual void	TraceRay( const Ray_t &ray, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace ) = 0;

	// A version that sets up the leaf and entity lists and allows you to pass those in for collision.
	virtual void	SetupLeafAndEntityListRay( const Ray_t &ray, ITraceListData *pTraceData ) = 0;
	virtual void    SetupLeafAndEntityListBox( const Vector &vecBoxMin, const Vector &vecBoxMax, ITraceListData *pTraceData ) = 0;
	virtual void	TraceRayAgainstLeafAndEntityList( const Ray_t &ray, ITraceListData *pTraceData, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace ) = 0;

	// A version that sweeps a collideable through the world
	// abs start + abs end represents the collision origins you want to sweep the collideable through
	// vecAngles represents the collision angles of the collideable during the sweep
	virtual void	SweepCollideable( ICollideable *pCollide, const Vector &vecAbsStart, const Vector &vecAbsEnd, 
		const QAngle &vecAngles, unsigned int fMask, ITraceFilter *pTraceFilter, trace_t *pTrace ) = 0;

	// Enumerates over all entities along a ray
	// If triggers == true, it enumerates all triggers along a ray
	virtual void	EnumerateEntities( const Ray_t &ray, bool triggers, IEntityEnumerator *pEnumerator ) = 0;

	// Same thing, but enumerate entitys within a box
	virtual void	EnumerateEntities( const Vector &vecAbsMins, const Vector &vecAbsMaxs, IEntityEnumerator *pEnumerator ) = 0;

	// Convert a handle entity to a collideable.  Useful inside enumer
	virtual ICollideable *GetCollideable( IHandleEntity *pEntity ) = 0;

	// HACKHACK: Temp for performance measurments
	virtual int GetStatByIndex( int index, bool bClear ) = 0;


	//finds brushes in an AABB, prone to some false positives
	virtual void GetBrushesInAABB( const Vector &vMins, const Vector &vMaxs, CBrushQuery &BrushQuery, int iContentsMask = 0xFFFFFFFF, int cmodelIndex = 0 ) = 0;

	//Creates a CPhysCollide out of all displacements wholly or partially contained in the specified AABB
	virtual CPhysCollide* GetCollidableFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs ) = 0;

	// gets the number of displacements in the world
	virtual int GetNumDisplacements( ) = 0;

	// gets a specific diplacement mesh
	virtual void GetDisplacementMesh( int nIndex, virtualmeshlist_t *pMeshTriList ) = 0;
	
	//retrieve brush planes and contents, returns zero if the brush doesn't exist, 
	//returns positive number of sides filled out if the array can hold them all, negative number of slots needed to hold info if the array is too small
	virtual int GetBrushInfo( int iBrush, int &ContentsOut, BrushSideInfo_t *pBrushSideInfoOut, int iBrushSideInfoArraySize ) = 0;

	virtual bool PointOutsideWorld( const Vector &ptTest ) = 0; //Tests a point to see if it's outside any playable area

	// Walks bsp to find the leaf containing the specified point
	virtual int GetLeafContainingPoint( const Vector &ptTest ) = 0;

	virtual ITraceListData *AllocTraceListData() = 0;
	virtual void FreeTraceListData(ITraceListData *) = 0;

	/// Used only in debugging: get/set/clear/increment the trace debug counter. See comment below for details.
	virtual int GetSetDebugTraceCounter( int value, DebugTraceCounterBehavior_t behavior ) = 0;

	//Similar to GetCollidableFromDisplacementsInAABB(). But returns the intermediate mesh data instead of a collideable
	virtual int GetMeshesFromDisplacementsInAABB( const Vector& vMins, const Vector& vMaxs, virtualmeshlist_t *pOutputMeshes, int iMaxOutputMeshes ) = 0;

	virtual void GetBrushesInCollideable( ICollideable *pCollideable, CBrushQuery &BrushQuery ) = 0;

	virtual bool IsFullyOccluded( int nOcclusionKey, const AABB_t &aabb1, const AABB_t &aabb2, const Vector &vShadow ) = 0;

	virtual void SuspendOcclusionTests() = 0;
	virtual void ResumeOcclusionTests() = 0;
	class CAutoSuspendOcclusionTests
	{
		IEngineTrace *m_pEngineTrace;
	public:
		CAutoSuspendOcclusionTests( IEngineTrace *pEngineTrace ) : m_pEngineTrace( pEngineTrace ) { pEngineTrace->SuspendOcclusionTests(); }
		~CAutoSuspendOcclusionTests() { m_pEngineTrace->ResumeOcclusionTests(); }
	};

	virtual void FlushOcclusionQueries() = 0;
};

/// IEngineTrace::GetSetDebugTraceCounter
/// SET to a negative number to disable. SET to a positive number to reset the counter; it'll tick down by
/// one for each trace, and break into the debugger on hitting zero. INC lets you add or subtract from the 
/// counter. In each case it will return the value of the counter BEFORE you set or incremented it. INC 0 
/// to query.  This end-around approach is necessary for security: because only the engine knows when we
/// are in a trace, and only the server knows when we are in a think, data must somehow be shared between
/// them. Simply returning a pointer to an address inside the engine in a retail build is unacceptable,
/// and in the PC there is no way to distinguish between retail and non-retail builds at compile time
/// (there is no #define for it).
/// This may seem redundant with the VPROF_INCREMENT_COUNTER( "TraceRay" ), but it's not, because while
/// that's readable across DLLs, there's no way to trap on its exceeding a certain value, nor can we reset
/// it for each think. 

#endif // ENGINE_IENGINETRACE_H
