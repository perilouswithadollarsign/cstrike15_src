//========= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=====================================================================================//


#include "cbase.h"
#include "portalsimulation.h"
#include "vphysics_interface.h"
#include "physics.h"
#include "portal_shareddefs.h"
#include "StaticCollisionPolyhedronCache.h"
#include "model_types.h"
#include "filesystem.h"
#include "collisionutils.h"
#include "tier1/callqueue.h"
#include "vphysics/virtualmesh.h"

#ifndef CLIENT_DLL

#include "world.h"
#include "portal_player.h" //TODO: Move any portal mod specific code to callback functions or something
#include "physicsshadowclone.h"
#include "portal/weapon_physcannon.h"
#include "player_pickup.h"
#include "isaverestore.h"
#include "hierarchy.h"
#include "env_debughistory.h"

#else

#include "c_world.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( CLIENT_DLL )
#define s_szDLLName "client"
#else
#define s_szDLLName "server"
#endif

CCallQueue *GetPortalCallQueue();

extern IPhysicsConstraintEvent *g_pConstraintEvents;

//#define DEBUG_PORTAL_SIMULATION_CREATION_TIMES //define to output creation timings to developer 2
#define DEBUG_PORTAL_COLLISION_ENVIRONMENTS //define this to allow for glview collision dumps of portal simulators

#define VPHYSICS_SHRINK	(0.5f) //HACK: assume VBSP uses this number until we have time to encode it in the map per model

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS ) || defined( DEBUG_PORTAL_SIMULATION_CREATION_TIMES )
#	if !defined( PORTAL_SIMULATORS_EMBED_GUID )
#		pragma message( __FILE__ "(" __LINE__AS_STRING ") : error custom: Portal simulators require a GUID to debug, enable the GUID in PortalSimulation.h ." )
#	endif
#endif

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS )
void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName ); //appends to the existing file if it exists
#endif

#define PORTAL_WALL_TUBE_DEPTH (1.0f) //(1.0f/128.0f)
#define PORTAL_WALL_TUBE_OFFSET (0.01f) //(1.0f/128.0f)
#define PORTAL_WALL_MIN_THICKNESS (0.1f) //(1.0f/16.0f)
#define PORTAL_POLYHEDRON_CUT_EPSILON (1.0f/1024.0f) //(1.0f/128.0f)
#define PORTAL_WORLDCLIP_EPSILON (1.0f/1024.0f) //(1.0f/256.0f)
#define PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT (1.0f/16.0f) //separating the world collision from wall collision by a small amount gets rid of extremely thin erroneous collision at the separating plane
#define PORTAL_HOLE_HALF_HEIGHT_MOD (0.1f)
#define PORTAL_HOLE_HALF_WIDTH_MOD (0.1f)

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
static ConVar sv_dump_portalsimulator_collision( "sv_dump_portalsimulator_collision", "0", FCVAR_REPLICATED | FCVAR_CHEAT ); //whether to actually dump out the data now that the possibility exists
static ConVar sv_dump_portalsimulator_holeshapes( "sv_dump_portalsimulator_holeshapes", "0", FCVAR_REPLICATED );
static void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );
static void PortalSimulatorDumps_DumpBoxToGlView( const Vector &vMins, const Vector &vMaxs, float fRed, float fGreen, float fBlue, const char *pszFileName );
#endif

#ifdef DEBUG_PORTAL_SIMULATION_CREATION_TIMES
#define STARTDEBUGTIMER(x) { x.Start(); }
#define STOPDEBUGTIMER(x) { x.End(); }
#define DEBUGTIMERONLY(x) x
#define CREATEDEBUGTIMER(x) CFastTimer x;
static const char *s_szTabSpacing[] = { "", "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t", "\t\t\t\t\t\t", "\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t\t\t\t" };
static int s_iTabSpacingIndex = 0;
static int s_iPortalSimulatorGUID = 0; //used in standalone function that have no idea what a portal simulator is
#define INCREMENTTABSPACING() ++s_iTabSpacingIndex;
#define DECREMENTTABSPACING() --s_iTabSpacingIndex;
#define TABSPACING (s_szTabSpacing[s_iTabSpacingIndex])
#else
#define STARTDEBUGTIMER(x)
#define STOPDEBUGTIMER(x)
#define DEBUGTIMERONLY(x)
#define CREATEDEBUGTIMER(x)
#define INCREMENTTABSPACING()
#define DECREMENTTABSPACING()
#define TABSPACING
#endif

static void ConvertBrushListToClippedPolyhedronList( const uint32 *pBrushes, int iBrushCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList );
static void ClipPolyhedrons( CPolyhedron * const *pExistingPolyhedrons, int iPolyhedronCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList );
static inline CPolyhedron *TransformAndClipSinglePolyhedron( CPolyhedron *pExistingPolyhedron, const VMatrix &Transform, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fCutEpsilon, bool bUseTempMemory );
static int GetEntityPhysicsObjects( IPhysicsEnvironment *pEnvironment, CBaseEntity *pEntity, IPhysicsObject **pRetList, int iRetListArraySize );
static CPhysCollide *ConvertPolyhedronsToCollideable( CPolyhedron **pPolyhedrons, int iPolyhedronCount );
static void CarveWallBrushes_Sub( float *fPlanes, CUtlVector<CPolyhedron *> &WallBrushPolyhedrons_ClippedToWall, PS_InternalData_t &InternalData, CUtlVector<CPolyhedron *> &OutputPolyhedrons, float fFarRightPlaneDistance, float fFarLeftPlaneDistance, const Vector &vLeft, const Vector &vDown );

#ifndef CLIENT_DLL
static void UpdateShadowClonesPortalSimulationFlags( const CBaseEntity *pSourceEntity, unsigned int iFlags, int iSourceFlags );
#endif

static CUtlVector<CPortalSimulator *> s_PortalSimulators;
CUtlVector<CPortalSimulator *> const &g_PortalSimulators = s_PortalSimulators;

static CPortalSimulator *s_OwnedEntityMap[MAX_EDICTS] = { NULL };
static CPortalSimulatorEventCallbacks s_DummyPortalSimulatorCallback;

const char *PS_SD_Static_World_StaticProps_ClippedProp_t::szTraceSurfaceName = "**studio**";
const int PS_SD_Static_World_StaticProps_ClippedProp_t::iTraceSurfaceFlags = 0;
CBaseEntity *PS_SD_Static_World_StaticProps_ClippedProp_t::pTraceEntity = NULL;

ConVar portal_clone_displacements ( "portal_clone_displacements", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar portal_environment_radius( "portal_environment_radius", "75", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar portal_ghosts_scale( "portal_ghosts_scale", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Scale the bounds of objects ghosted in portal environments for the purposes of hit testing." );
ConVar portal_ghost_force_hitbox("portal_ghost_force_hitbox", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "(1 = Legacy behavior) Force potentially ghosted renderables to use their hitboxes to test against portal holes instead of collision AABBs" );
ConVar portal_ghost_show_bbox("portal_ghost_show_bbox", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Render AABBs around the bounding box used for ghost renderable bounds checking (either hitbox or collision AABB)" );


#if defined( GAME_DLL )
ConVar portal_carve_vphysics_clips( "portal_carve_vphysics_clips", "1" );

class CFunc_VPhysics_Clip_Watcher : public CAutoGameSystem
{
public:
	CFunc_VPhysics_Clip_Watcher( void )
	{
		m_bHaveCached = false;
	}
	virtual void LevelInitPostEntity()
	{
		Cache();
	}

	virtual void LevelShutdownPostEntity()
	{
		m_VPhysicsClipEntities.RemoveAll();
		m_bHaveCached = false;
	}

	void Cache( void )
	{
		if( m_bHaveCached )
			return;

		CBaseEntity *pIterateEntities = NULL;
		while( (pIterateEntities = gEntList.FindEntityByClassname( pIterateEntities, "func_clip_vphysics" )) != NULL )
		{
			CCollisionProperty *pProp = pIterateEntities->CollisionProp();

			VPhysicsClipEntry_t tempEntry;
			tempEntry.hEnt = pIterateEntities;

			pProp->WorldSpaceAABB( &tempEntry.vAABBMins, &tempEntry.vAABBMaxs );
			m_VPhysicsClipEntities.AddToTail( tempEntry );
		}

		m_bHaveCached = true;
	}


	CUtlVector<VPhysicsClipEntry_t> m_VPhysicsClipEntities;
	bool m_bHaveCached;
};
static CFunc_VPhysics_Clip_Watcher s_VPhysicsClipWatcher;

CUtlVector<VPhysicsClipEntry_t>& GetVPhysicsClipList ( void )
{
	return s_VPhysicsClipWatcher.m_VPhysicsClipEntities;
}
#endif


#if defined( DBGFLAG_ASSERT ) && 0 //only enable this if mathlib.lib is built with DBGFLAG_ASSERT and ENABLE_DEBUG_POLYHEDRON_DUMPS is defined in polyhedron.cpp

extern void DumpPolyhedronToGLView( const CPolyhedron *pPolyhedron, const char *pFilename, const VMatrix *pTransform, const char *szfileOpenOptions = "ab" ); //need to make sure mathlib creates this by building it debug or with DBGFLAG_ASSERT

typedef bool (*PFN_PolyhedronCarvingDebugStepCallback)( CPolyhedron *pPolyhedron ); //function that receives a polyhedron conversion after each cut. For the slowest, surest debugging possible. Returns true if the polyhedron passes mustard, false to dump the current work state
extern PFN_PolyhedronCarvingDebugStepCallback g_pPolyhedronCarvingDebugStepCallback;
#define DEBUG_POLYHEDRON_CONVERSION 1

bool TestPolyhedronConversion( CPolyhedron *pPolyhedron )
{
	if( pPolyhedron == NULL )
		return false;

	//dump each test case
	if( false )
	{
		VMatrix matScaleNearOrigin;
		matScaleNearOrigin.Identity();
		const float cScale = 10.0f;
		matScaleNearOrigin = matScaleNearOrigin.Scale( Vector( cScale, cScale, cScale ) );
		matScaleNearOrigin.SetTranslation( -pPolyhedron->Center() * cScale );
#ifndef CLIENT_DLL
		const char *szDumpFile = "TestPolyhedronConversionServer.txt";
#else
		const char *szDumpFile = "TestPolyhedronConversionClient.txt";
#endif

		DumpPolyhedronToGLView( pPolyhedron, szDumpFile, &matScaleNearOrigin, "wb" );
	}

	CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
	if( pConvex == NULL )
		return false;

	//TODO: is there an easier way to destroy the convex directly without converting it to a collide first? Debug only code, do we care to make something new?
	CPhysCollide *pCollide = physcollision->ConvertConvexToCollide( &pConvex, 1 );
	physcollision->DestroyCollide( pCollide );

	return true;
}

#endif


#if defined( CLIENT_DLL )
//copy/paste from game/server/hierarchy.cpp
static void GetAllChildren_r( CBaseEntity *pEntity, CUtlVector<CBaseEntity *> &list )
{
	for ( ; pEntity != NULL; pEntity = pEntity->NextMovePeer() )
	{
		list.AddToTail( pEntity );
		GetAllChildren_r( pEntity->FirstMoveChild(), list );
	}
}

int GetAllChildren( CBaseEntity *pParent, CUtlVector<CBaseEntity *> &list )
{
	if ( !pParent )
		return 0;

	GetAllChildren_r( pParent->FirstMoveChild(), list );
	return list.Count();
}
#endif

#ifdef GAME_DLL
BEGIN_SEND_TABLE_NOBASE( PS_SimulationData_t, DT_PS_SimulationData_t )
	SendPropEHandle( SENDINFO( hCollisionEntity ) )
END_SEND_TABLE()
#else
BEGIN_RECV_TABLE_NOBASE( PS_SimulationData_t, DT_PS_SimulationData_t )
	RecvPropEHandle( RECVINFO( hCollisionEntity ) )
END_RECV_TABLE()
#endif // ifdef GAME_DLL

#ifdef GAME_DLL
BEGIN_SEND_TABLE_NOBASE( PS_InternalData_t, DT_PS_InternalData_t )
	SendPropDataTable( SENDINFO_DT(Simulation), &REFERENCE_SEND_TABLE(DT_PS_SimulationData_t) )
END_SEND_TABLE()
#else
BEGIN_RECV_TABLE_NOBASE( PS_InternalData_t, DT_PS_InternalData_t )
	RecvPropDataTable( RECVINFO_DT(Simulation), 0, &REFERENCE_RECV_TABLE(DT_PS_SimulationData_t) )
END_RECV_TABLE()
#endif // ifdef GAME_DLL

#ifdef GAME_DLL
BEGIN_SEND_TABLE_NOBASE( CPortalSimulator, DT_PortalSimulator )
	SendPropDataTable( SENDINFO_DT(m_InternalData), &REFERENCE_SEND_TABLE(DT_PS_InternalData_t) )
END_SEND_TABLE()
#else
BEGIN_RECV_TABLE_NOBASE( CPortalSimulator, DT_PortalSimulator )
	RecvPropDataTable( RECVINFO_DT(m_InternalData), 0, &REFERENCE_RECV_TABLE(DT_PS_InternalData_t) )
END_RECV_TABLE()
#endif // ifdef GAME_DLL

CPortalSimulator::CPortalSimulator( void )
: m_bLocalDataIsReady(false),
	m_bGenerateCollision(true),
	m_bSimulateVPhysics(true),
	m_bSharedCollisionConfiguration(false),
	m_pLinkedPortal(NULL),
	m_bInCrossLinkedFunction(false),
	m_pCallbacks(&s_DummyPortalSimulatorCallback)
{
	s_PortalSimulators.AddToTail( this );

#if defined( DEBUG_POLYHEDRON_CONVERSION )
	g_pPolyhedronCarvingDebugStepCallback = TestPolyhedronConversion;
#endif

#ifdef CLIENT_DLL
	m_bGenerateCollision = (GameRules() && GameRules()->IsMultiplayer());
#endif

	m_CreationChecklist.bPolyhedronsGenerated = false;
	m_CreationChecklist.bLocalCollisionGenerated = false;
	m_CreationChecklist.bLinkedCollisionGenerated = false;
	m_CreationChecklist.bLocalPhysicsGenerated = false;
	m_CreationChecklist.bLinkedPhysicsGenerated = false;

#ifdef PORTAL_SIMULATORS_EMBED_GUID
	static int s_iPortalSimulatorGUIDAllocator = 0;
	m_iPortalSimulatorGUID = s_iPortalSimulatorGUIDAllocator++;
#endif

#ifndef CLIENT_DLL
	PS_SD_Static_World_StaticProps_ClippedProp_t::pTraceEntity = GetWorldEntity(); //will overinitialize, but it's cheap

	m_InternalData.Simulation.hCollisionEntity = (CPSCollisionEntity *)CreateEntityByName( "portalsimulator_collisionentity" );
	Assert( m_InternalData.Simulation.hCollisionEntity != NULL );
	if( m_InternalData.Simulation.hCollisionEntity )
	{
		m_InternalData.Simulation.hCollisionEntity->m_pOwningSimulator = this;
		MarkAsOwned( m_InternalData.Simulation.hCollisionEntity );
		m_InternalData.Simulation.Dynamic.EntFlags[m_InternalData.Simulation.hCollisionEntity->entindex()] |= PSEF_OWNS_PHYSICS;
		DispatchSpawn( m_InternalData.Simulation.hCollisionEntity );
	}
#else
	PS_SD_Static_World_StaticProps_ClippedProp_t::pTraceEntity = GetClientWorldEntity();
#endif
}



CPortalSimulator::~CPortalSimulator( void )
{
	//go assert crazy here
	DetachFromLinked();
	ClearEverything();

	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		if( s_PortalSimulators[i] == this )
		{
			s_PortalSimulators.FastRemove( i );
			break;
		}
	}

	if( m_InternalData.Placement.pHoleShapeCollideable )
		physcollision->DestroyCollide( m_InternalData.Placement.pHoleShapeCollideable );

	if( m_InternalData.Placement.pInvHoleShapeCollideable )
		physcollision->DestroyCollide( m_InternalData.Placement.pInvHoleShapeCollideable );

	if( m_InternalData.Placement.pAABBAngleTransformCollideable )
		physcollision->DestroyCollide( m_InternalData.Placement.pAABBAngleTransformCollideable );

	
	
#ifndef CLIENT_DLL
	if( m_InternalData.Simulation.hCollisionEntity )
	{
		m_InternalData.Simulation.hCollisionEntity->m_pOwningSimulator = NULL;
		m_InternalData.Simulation.Dynamic.EntFlags[m_InternalData.Simulation.hCollisionEntity->entindex()] &= ~PSEF_OWNS_PHYSICS;
		MarkAsReleased( m_InternalData.Simulation.hCollisionEntity );
		UTIL_Remove( m_InternalData.Simulation.hCollisionEntity );
		m_InternalData.Simulation.hCollisionEntity = NULL;
	}
#endif
}

void CPortalSimulator::SetSize( float fHalfWidth, float fHalfHeight )
{
	if( (m_InternalData.Placement.fHalfWidth == fHalfWidth) && (m_InternalData.Placement.fHalfHeight == fHalfHeight) ) //not actually resizing at all
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::SetSize() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	MovedOrResized( m_InternalData.Placement.ptCenter, m_InternalData.Placement.qAngles, fHalfWidth, fHalfHeight );

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::SetSize() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}

void CPortalSimulator::MoveTo( const Vector &ptCenter, const QAngle &angles )
{
	if( (m_InternalData.Placement.ptCenter == ptCenter) && (m_InternalData.Placement.qAngles == angles) ) //not actually moving at all
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::MoveTo() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	MovedOrResized( ptCenter, angles, m_InternalData.Placement.fHalfWidth, m_InternalData.Placement.fHalfHeight );

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::MoveTo() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}

extern ConVar sv_portal_new_player_trace;

CEG_NOINLINE void CPortalSimulator::MovedOrResized( const Vector &ptCenter, const QAngle &qAngles, float fHalfWidth, float fHalfHeight )
{
	if( (fHalfWidth == 0.0f) || (fHalfHeight == 0.0f) || !ptCenter.IsValid() )
	{
		m_InternalData.Placement.fHalfWidth = fHalfWidth;
		m_InternalData.Placement.fHalfHeight = fHalfHeight;

		ClearEverything();
		return;
	}

#ifndef CLIENT_DLL
	//create a list of all entities that are actually within the portal hole, they will likely need to be moved out of solid space when the portal moves
	CBaseEntity **pFixEntities = (CBaseEntity **)stackalloc( sizeof( CBaseEntity * ) * m_InternalData.Simulation.Dynamic.OwnedEntities.Count() );
	int iFixEntityCount = 0;
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
	{
		CBaseEntity *pEntity = m_InternalData.Simulation.Dynamic.OwnedEntities[i];
		if( CPhysicsShadowClone::IsShadowClone( pEntity ) ||
			CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
			continue;

		if( EntityIsInPortalHole( pEntity ) )
		{
			pFixEntities[iFixEntityCount] = pEntity;
			++iFixEntityCount;
		}
	}
	VPlane OldPlane = m_InternalData.Placement.PortalPlane; //used in fixing code
#endif

	//update placement data
	{
		m_InternalData.Placement.ptCenter = ptCenter;
		m_InternalData.Placement.qAngles = qAngles;
		AngleVectors( qAngles, &m_InternalData.Placement.vForward, &m_InternalData.Placement.vRight, &m_InternalData.Placement.vUp );

		m_InternalData.Placement.PortalPlane.Init( m_InternalData.Placement.vForward, m_InternalData.Placement.vForward.Dot( m_InternalData.Placement.ptCenter ) );

		m_InternalData.Placement.fHalfWidth = fHalfWidth;
		m_InternalData.Placement.fHalfHeight = fHalfHeight;

		m_InternalData.Placement.vCollisionCloneExtents.x = MAX( fHalfWidth, fHalfHeight ) + portal_environment_radius.GetFloat();
		m_InternalData.Placement.vCollisionCloneExtents.y = fHalfWidth + portal_environment_radius.GetFloat();
		m_InternalData.Placement.vCollisionCloneExtents.z = fHalfHeight + portal_environment_radius.GetFloat();
	}

	//Clear();
#ifndef CLIENT_DLL
	ClearLinkedPhysics();
	ClearLocalPhysics();
#endif
	ClearLinkedCollision();
	ClearLocalCollision();
	ClearPolyhedrons();

	m_bLocalDataIsReady = true;
	UpdateLinkMatrix();

	//update hole shape - used to detect if an entity is within the portal hole bounds
	{
		float fHolePlanes[6*4];

		//first and second planes are always forward and backward planes
		fHolePlanes[(0*4) + 0] = m_InternalData.Placement.PortalPlane.m_Normal.x;
		fHolePlanes[(0*4) + 1] = m_InternalData.Placement.PortalPlane.m_Normal.y;
		fHolePlanes[(0*4) + 2] = m_InternalData.Placement.PortalPlane.m_Normal.z;
		fHolePlanes[(0*4) + 3] = m_InternalData.Placement.PortalPlane.m_Dist - 0.5f;

		fHolePlanes[(1*4) + 0] = -m_InternalData.Placement.PortalPlane.m_Normal.x;
		fHolePlanes[(1*4) + 1] = -m_InternalData.Placement.PortalPlane.m_Normal.y;
		fHolePlanes[(1*4) + 2] = -m_InternalData.Placement.PortalPlane.m_Normal.z;
		fHolePlanes[(1*4) + 3] = (-m_InternalData.Placement.PortalPlane.m_Dist) + 500.0f;


		//the remaining planes will always have the same ordering of normals, with different distances plugged in for each convex we're creating
		//normal order is up, down, left, right

		fHolePlanes[(2*4) + 0] = m_InternalData.Placement.vUp.x;
		fHolePlanes[(2*4) + 1] = m_InternalData.Placement.vUp.y;
		fHolePlanes[(2*4) + 2] = m_InternalData.Placement.vUp.z;
		fHolePlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (m_InternalData.Placement.fHalfHeight * 0.98f)) );

		fHolePlanes[(3*4) + 0] = -m_InternalData.Placement.vUp.x;
		fHolePlanes[(3*4) + 1] = -m_InternalData.Placement.vUp.y;
		fHolePlanes[(3*4) + 2] = -m_InternalData.Placement.vUp.z;
		fHolePlanes[(3*4) + 3] = -m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vUp * (m_InternalData.Placement.fHalfHeight * 0.98f)) );

		fHolePlanes[(4*4) + 0] = -m_InternalData.Placement.vRight.x;
		fHolePlanes[(4*4) + 1] = -m_InternalData.Placement.vRight.y;
		fHolePlanes[(4*4) + 2] = -m_InternalData.Placement.vRight.z;
		fHolePlanes[(4*4) + 3] = -m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth * 0.98f)) );

		fHolePlanes[(5*4) + 0] = m_InternalData.Placement.vRight.x;
		fHolePlanes[(5*4) + 1] = m_InternalData.Placement.vRight.y;
		fHolePlanes[(5*4) + 2] = m_InternalData.Placement.vRight.z;
		fHolePlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth * 0.98f)) );

		//create hole collideable
		{
			if( m_InternalData.Placement.pHoleShapeCollideable )
				physcollision->DestroyCollide( m_InternalData.Placement.pHoleShapeCollideable );

			CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fHolePlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON, true );
			Assert( pPolyhedron != NULL );
			CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
			pPolyhedron->Release();
			Assert( pConvex != NULL );
			convertconvexparams_t params;
			params.Defaults();
			params.buildOptimizedTraceTables = true;
			params.bUseFastApproximateInertiaTensor = true;
			m_InternalData.Placement.pHoleShapeCollideable = physcollision->ConvertConvexToCollideParams( &pConvex, 1, params );
		}

		//create inverse hole collideable
		{
			if( m_InternalData.Placement.pInvHoleShapeCollideable )
				physcollision->DestroyCollide( m_InternalData.Placement.pInvHoleShapeCollideable );

			if( m_InternalData.Placement.pAABBAngleTransformCollideable )
				physcollision->DestroyCollide( m_InternalData.Placement.pAABBAngleTransformCollideable );

			const float kCarveEpsilon = (1.0f / 512.0f);
			//make thickness extra thin
			fHolePlanes[(0*4) + 3] = m_InternalData.Placement.PortalPlane.m_Dist;
			fHolePlanes[(1*4) + 3] = (-m_InternalData.Placement.PortalPlane.m_Dist) + 1.0f;

			float fAABBTransformPlanes[6*4];
			memcpy( fAABBTransformPlanes, fHolePlanes, sizeof( float ) * 6 * 4 );
			fAABBTransformPlanes[(0*4) + 3] = m_InternalData.Placement.PortalPlane.m_Dist - (PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT / 2.0f);
			fAABBTransformPlanes[(1*4) + 3] = (-m_InternalData.Placement.PortalPlane.m_Dist) + (64.0f);

			//set initial outer bounds super far away (supposed to represent an infinite plane with a finite solid)
			const float kReallyFar = 1024.0f;
			float fFarDists[4]; //mapping is meant to be (fFarDists[i] <-> fHolePlanes[((i+2)*4) + 3])
			fFarDists[0] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * kReallyFar) );
			fFarDists[1] = -m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vUp * kReallyFar) );
			fFarDists[2] = -m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vRight * kReallyFar) );
			fFarDists[3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vRight * kReallyFar) );

#ifdef CLIENT_DLL
			CEG_PROTECT_MEMBER_FUNCTION( CPortalSimulator_MovedOrResized );
#endif

			const float kInnerCarve = 0.1f;
			float fInvHoleNearDists[4]; //mapping is meant to be (fInvHoleNearDists[i] <-> fHolePlanes[((i+2)*4) + 3])
			fInvHoleNearDists[0] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vUp * (m_InternalData.Placement.fHalfHeight + kInnerCarve)) );
			fInvHoleNearDists[1] = -m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (m_InternalData.Placement.fHalfHeight + kInnerCarve)) );
			fInvHoleNearDists[2] = -m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth + kInnerCarve)) );
			fInvHoleNearDists[3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth + kInnerCarve)) );

			const float kAABBInnerCarve = (PORTAL_HOLE_HALF_WIDTH_MOD + (1.0f/16.0f)) * 4.0f;//(-1.0f/1024.0f);
			float fAABBTransformNearDists[4]; //mapping is meant to be (fAABBTransformNearDists[i] <-> fAABBTransformPlanes[((i+2)*4) + 3])
			fAABBTransformNearDists[0] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vUp * (m_InternalData.Placement.fHalfHeight + kAABBInnerCarve)) );
			fAABBTransformNearDists[1] = -m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vUp * (m_InternalData.Placement.fHalfHeight + kAABBInnerCarve)) );
			fAABBTransformNearDists[2] = -m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + (m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth + kAABBInnerCarve)) );
			fAABBTransformNearDists[3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth + kAABBInnerCarve)) );

			//left and right sections will be the sliver segments, top and bottom are roughly half the surface area of the entire collideable each
			CPhysConvex *pInvHoleConvexes[4];
			CPhysConvex *pAABBTransformConvexes[4];

			//top section
			{
				fHolePlanes[(2*4) + 3] = fFarDists[0];
				fHolePlanes[(3*4) + 3] = fInvHoleNearDists[1];
				fHolePlanes[(4*4) + 3] = fFarDists[2];
				fHolePlanes[(5*4) + 3] = fFarDists[3];

				CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fHolePlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pInvHoleConvexes[0] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pInvHoleConvexes[0] != NULL );



				fAABBTransformPlanes[(2*4) + 3] = fFarDists[0];
				fAABBTransformPlanes[(3*4) + 3] = fAABBTransformNearDists[1];
				fAABBTransformPlanes[(4*4) + 3] = fFarDists[2];
				fAABBTransformPlanes[(5*4) + 3] = fFarDists[3];

				/*pPolyhedron = GeneratePolyhedronFromPlanes( fAABBTransformPlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pAABBTransformConvexes[0] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pAABBTransformConvexes[0] != NULL );*/
			}

			//bottom section
			{
				fHolePlanes[(2*4) + 3] = fInvHoleNearDists[0];
				fHolePlanes[(3*4) + 3] = fFarDists[1];
				//fHolePlanes[(4*4) + 3] = fFarDists[2]; //no change since top section
				//fHolePlanes[(5*4) + 3] = fFarDists[3];

				CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fHolePlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pInvHoleConvexes[1] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pInvHoleConvexes[1] != NULL );

				

				fAABBTransformPlanes[(2*4) + 3] = fAABBTransformNearDists[0];
				fAABBTransformPlanes[(3*4) + 3] = fFarDists[1];
				//fAABBTransformPlanes[(4*4) + 3] = fFarDists[2]; //no change since top section
				//fAABBTransformPlanes[(5*4) + 3] = fFarDists[3];

				pPolyhedron = GeneratePolyhedronFromPlanes( fAABBTransformPlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pAABBTransformConvexes[1] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pAABBTransformConvexes[1] != NULL );
			}

			//left section
			{
				fHolePlanes[(2*4) + 3] = -fInvHoleNearDists[1]; //remap inward facing top/bottom near distances to outward facing ones
				fHolePlanes[(3*4) + 3] = -fInvHoleNearDists[0];
				//fHolePlanes[(4*4) + 3] = fFarDists[2];  //no change since bottom section
				fHolePlanes[(5*4) + 3] = fInvHoleNearDists[3];

				CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fHolePlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pInvHoleConvexes[2] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pInvHoleConvexes[2] != NULL );


				fAABBTransformPlanes[(2*4) + 3] = -fAABBTransformNearDists[1]; //remap inward facing top/bottom near distances to outward facing ones
				fAABBTransformPlanes[(3*4) + 3] = -fAABBTransformNearDists[0];
				//fAABBTransformPlanes[(4*4) + 3] = fFarDists[2];  //no change since bottom section
				fAABBTransformPlanes[(5*4) + 3] = fAABBTransformNearDists[3];

				/*pPolyhedron = GeneratePolyhedronFromPlanes( fAABBTransformPlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pAABBTransformConvexes[2] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pAABBTransformConvexes[2] != NULL );*/
			}

			//right section
			{
				//fHolePlanes[(2*4) + 3] = -fInvHoleNearDists[1]; //no change since left section
				//fHolePlanes[(3*4) + 3] = -fInvHoleNearDists[0];
				fHolePlanes[(4*4) + 3] = fInvHoleNearDists[2]; 
				fHolePlanes[(5*4) + 3] = fFarDists[3];

				CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( fHolePlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pInvHoleConvexes[3] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pInvHoleConvexes[3] != NULL );


				//fAABBTransformPlanes[(2*4) + 3] = -fAABBTransformNearDists[1]; //no change since left section
				//fAABBTransformPlanes[(3*4) + 3] = -fAABBTransformNearDists[0];
				fAABBTransformPlanes[(4*4) + 3] = fAABBTransformNearDists[2]; 
				fAABBTransformPlanes[(5*4) + 3] = fFarDists[3];

				/*pPolyhedron = GeneratePolyhedronFromPlanes( fAABBTransformPlanes, 6, kCarveEpsilon, true );
				Assert( pPolyhedron != NULL );
				pAABBTransformConvexes[3] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedron );
				pPolyhedron->Release();
				Assert( pAABBTransformConvexes[3] != NULL );*/
			}

			convertconvexparams_t params;
			params.Defaults();
			params.buildOptimizedTraceTables = true;
			params.bUseFastApproximateInertiaTensor = true;
			m_InternalData.Placement.pInvHoleShapeCollideable = physcollision->ConvertConvexToCollideParams( pInvHoleConvexes, 4, params );
		
			//m_InternalData.Placement.pAABBAngleTransformCollideable = physcollision->ConvertConvexToCollide( pAABBTransformConvexes, 4 );
			m_InternalData.Placement.pAABBAngleTransformCollideable = physcollision->ConvertConvexToCollideParams( &pAABBTransformConvexes[1], 1, params );
		}
	}

#ifndef CLIENT_DLL
	for( int i = 0; i != iFixEntityCount; ++i )
	{
		if( !EntityIsInPortalHole( pFixEntities[i] ) )
		{
			//this entity is most definitely stuck in a solid wall right now
			//pFixEntities[i]->SetAbsOrigin( pFixEntities[i]->GetAbsOrigin() + (OldPlane.m_Normal * 50.0f) );
			FindClosestPassableSpace( pFixEntities[i], OldPlane.m_Normal );
			continue;
		}

		//entity is still in the hole, but it's possible the hole moved enough where they're in part of the wall
		{
			//TODO: figure out if that's the case and fix it
		}
	}
#endif

	CreatePolyhedrons();	
	CreateAllCollision();
#ifndef CLIENT_DLL
	CreateAllPhysics();
#endif

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS )
	if( sv_dump_portalsimulator_collision.GetBool() )
	{
		const char *szFileName = "pscd_" s_szDLLName ".txt";
		filesystem->RemoveFile( szFileName );
		DumpActiveCollision( this, szFileName );
		if( m_pLinkedPortal )
		{
			szFileName = "pscd_" s_szDLLName "_linked.txt";
			filesystem->RemoveFile( szFileName );
			DumpActiveCollision( m_pLinkedPortal, szFileName );
		}
	}
#endif

#ifndef CLIENT_DLL
	Assert( (m_InternalData.Simulation.hCollisionEntity == NULL) || OwnsEntity(m_InternalData.Simulation.hCollisionEntity) );
#endif
}


void CPortalSimulator::UpdateLinkMatrix( void )
{
	if( m_pLinkedPortal && m_pLinkedPortal->m_bLocalDataIsReady )
	{
		Vector vLocalLeft = -m_InternalData.Placement.vRight;
		VMatrix matLocalToWorld( m_InternalData.Placement.vForward, vLocalLeft, m_InternalData.Placement.vUp );
		matLocalToWorld.SetTranslation( m_InternalData.Placement.ptCenter );

		VMatrix matLocalToWorldInverse;
		MatrixInverseTR( matLocalToWorld,matLocalToWorldInverse );

		//180 degree rotation about up
		VMatrix matRotation;
		matRotation.Identity();
		matRotation.m[0][0] = -1.0f;
		matRotation.m[1][1] = -1.0f;

		Vector vRemoteLeft = -m_pLinkedPortal->m_InternalData.Placement.vRight;
		VMatrix matRemoteToWorld( m_pLinkedPortal->m_InternalData.Placement.vForward, vRemoteLeft, m_pLinkedPortal->m_InternalData.Placement.vUp );
		matRemoteToWorld.SetTranslation( m_pLinkedPortal->m_InternalData.Placement.ptCenter );	

		//final
		m_InternalData.Placement.matThisToLinked = matRemoteToWorld * matRotation * matLocalToWorldInverse;
	}
	else
	{
		m_InternalData.Placement.matThisToLinked.Identity();
	}
	
	m_InternalData.Placement.matThisToLinked.InverseTR( m_InternalData.Placement.matLinkedToThis );

	MatrixAngles( m_InternalData.Placement.matThisToLinked.As3x4(), m_InternalData.Placement.ptaap_ThisToLinked.qAngleTransform, m_InternalData.Placement.ptaap_ThisToLinked.ptOriginTransform );
	MatrixAngles( m_InternalData.Placement.matLinkedToThis.As3x4(), m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform, m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform );

	m_InternalData.Placement.ptaap_ThisToLinked.ptShrinkAlignedOrigin = m_InternalData.Placement.ptaap_ThisToLinked.ptOriginTransform;
	m_InternalData.Placement.ptaap_LinkedToThis.ptShrinkAlignedOrigin = m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform;

	if( m_InternalData.Placement.bParentIsVPhysicsSolidBrush )
	{
		if( m_pLinkedPortal )
		{
			m_InternalData.Placement.ptaap_ThisToLinked.ptShrinkAlignedOrigin += m_pLinkedPortal->m_InternalData.Placement.vForward * VPHYSICS_SHRINK;
		}
		m_InternalData.Placement.ptaap_LinkedToThis.ptShrinkAlignedOrigin -= m_InternalData.Placement.vForward * VPHYSICS_SHRINK;
	}


	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->UpdateLinkMatrix();
		m_bInCrossLinkedFunction = false;
	}
}

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
static ConVar sv_debug_dumpportalhole_nextcheck( "sv_debug_dumpportalhole_nextcheck", "0", FCVAR_CHEAT | FCVAR_REPLICATED );
#endif

bool CPortalSimulator::EntityIsInPortalHole( CBaseEntity *pEntity ) const
{
	if( m_bLocalDataIsReady == false )
		return false;

	Assert( m_InternalData.Placement.pHoleShapeCollideable != NULL );

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
	const char *szDumpFileName = "ps_entholecheck.txt";
	if( sv_debug_dumpportalhole_nextcheck.GetBool() )
	{
		filesystem->RemoveFile( szDumpFileName );

		DumpActiveCollision( this, szDumpFileName );
		PortalSimulatorDumps_DumpCollideToGlView( m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, 1.0f, szDumpFileName );
	}
#endif

	trace_t Trace;

	switch( pEntity->GetSolid() )
	{
	case SOLID_VPHYSICS:
		{
			ICollideable *pCollideable = pEntity->GetCollideable();
			vcollide_t *pVCollide = modelinfo->GetVCollide( pCollideable->GetCollisionModel() );
			
			//Assert( pVCollide != NULL ); //brush models?
			if( pVCollide != NULL )
			{
				Vector ptEntityPosition = pCollideable->GetCollisionOrigin();
				QAngle qEntityAngles = pCollideable->GetCollisionAngles();

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
				if( sv_debug_dumpportalhole_nextcheck.GetBool() )
				{
					for( int i = 0; i != pVCollide->solidCount; ++i )
						PortalSimulatorDumps_DumpCollideToGlView( m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, 0.4f, szDumpFileName );
				
					sv_debug_dumpportalhole_nextcheck.SetValue( false );
				}
#endif

				for( int i = 0; i != pVCollide->solidCount; ++i )
				{
					physcollision->TraceCollide( ptEntityPosition, ptEntityPosition, pVCollide->solids[i], qEntityAngles, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

					if( Trace.startsolid )
						return true;
				}
			}
			else
			{
				//energy balls lack a vcollide
				Vector vMins, vMaxs, ptCenter;
				pCollideable->WorldSpaceSurroundingBounds( &vMins, &vMaxs );
				ptCenter = (vMins + vMaxs) * 0.5f;
				vMins -= ptCenter;
				vMaxs -= ptCenter;
				physcollision->TraceBox( ptCenter, ptCenter, vMins, vMaxs, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

				return Trace.startsolid;
			}
			break;
		}

	case SOLID_BBOX:
	case SOLID_OBB:
	case SOLID_OBB_YAW:
		{
#if defined( CLIENT_DLL )
			if( !C_BaseEntity::IsAbsQueriesValid() )
			{
				return ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_IS_IN_PORTAL_HOLE) != 0); //return existing value if we can't test it right now
			}
#endif
			Vector ptEntityPosition = pEntity->GetAbsOrigin();
			CCollisionProperty *pCollisionProp = pEntity->CollisionProp();

			physcollision->TraceBox( ptEntityPosition, ptEntityPosition, pCollisionProp->OBBMins(), pCollisionProp->OBBMaxs(), m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
			if( sv_debug_dumpportalhole_nextcheck.GetBool() )
			{
				Vector vMins = ptEntityPosition + pCollisionProp->OBBMins();
				Vector vMaxs = ptEntityPosition + pCollisionProp->OBBMaxs();
				PortalSimulatorDumps_DumpBoxToGlView( vMins, vMaxs, 1.0f, 1.0f, 1.0f, szDumpFileName );

				sv_debug_dumpportalhole_nextcheck.SetValue( false );
			}
#endif

			if( Trace.startsolid )
				return true;

			break;
		}
	case SOLID_NONE:
#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
		if( sv_debug_dumpportalhole_nextcheck.GetBool() )
			sv_debug_dumpportalhole_nextcheck.SetValue( false );
#endif

		return false;
	case SOLID_CUSTOM:
		{
			Vector vMins, vMaxs;
			Vector ptCenter = pEntity->CollisionProp()->GetCollisionOrigin();
			pEntity->ComputeWorldSpaceSurroundingBox( &vMins, &vMaxs );
			physcollision->TraceBox( ptCenter, ptCenter, vMins, vMaxs, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

		}
		break;

	default:
		Assert( false ); //make a handler
	};

#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS
	if( sv_debug_dumpportalhole_nextcheck.GetBool() )
		sv_debug_dumpportalhole_nextcheck.SetValue( false );
#endif

	return false;
}

bool CPortalSimulator::EntityHitBoxExtentIsInPortalHole( CBaseAnimating *pBaseAnimating, bool bUseCollisionAABB ) const
{
	if( m_bLocalDataIsReady == false )
		return false;

	Vector vMinsOut, vMaxsOut;
	Vector vCenter;

	if ( !bUseCollisionAABB || portal_ghost_force_hitbox.GetBool() )
	{
		CStudioHdr *pStudioHdr = pBaseAnimating->GetModelPtr();
		if ( !pStudioHdr )
			return false;

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( pBaseAnimating->m_nHitboxSet );
		if ( !set )
			return false;

		matrix3x4_t matTransform;
		Vector vMins, vMaxs;
		for ( int i = 0; i < set->numhitboxes; i++ )
		{
			mstudiobbox_t *pbox = set->pHitbox( i );
			
			pBaseAnimating->GetBoneTransform( pbox->bone, matTransform );
			TransformAABB( matTransform, pbox->bbmin, pbox->bbmax, vMins, vMaxs );
			if ( i == 0 )
			{
				vMinsOut = vMins;
				vMaxsOut = vMaxs;
			}
			else
			{
				vMinsOut = vMinsOut.Min( vMins );
				vMaxsOut = vMaxsOut.Max( vMaxs );
			}
		}
		vCenter = (vMinsOut + vMaxsOut) * 0.5f;
		vMinsOut -= vCenter;
		vMaxsOut -= vCenter;

#ifdef CLIENT_DLL
		// offset the center to render origin
		Vector vOffset = pBaseAnimating->GetRenderOrigin() - pBaseAnimating->GetAbsOrigin();
		vCenter += vOffset;
#endif // CLIENT_DLL
	}
	else
	{
		CCollisionProperty *pCollisionProp = pBaseAnimating->CollisionProp();
		pCollisionProp->WorldSpaceAABB( &vMinsOut, &vMaxsOut);

		vCenter = (vMinsOut + vMaxsOut) * 0.5f;
		vMinsOut -= vCenter;
		vMaxsOut -= vCenter;
	}

#ifdef CLIENT_DLL
	if ( portal_ghost_show_bbox.GetBool() )
	{
		NDebugOverlay::BoxAngles( vCenter, vMinsOut, vMaxsOut, vec3_angle, 200, 200, 50, 50, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	}
#endif // CLIENT_DLL

	float flScaleFactor = portal_ghosts_scale.GetFloat();
	vMinsOut *= flScaleFactor;
	vMaxsOut *= flScaleFactor;

	trace_t Trace;
	physcollision->TraceBox( vCenter, vCenter, vMinsOut, vMaxsOut, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

	if( Trace.startsolid )
		return true;

	return false;
}

void CPortalSimulator::RemoveEntityFromPortalHole( CBaseEntity *pEntity )
{
	switch( pEntity->GetMoveType() )
	{
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_LADDER:
	case MOVETYPE_OBSERVER:
	case MOVETYPE_CUSTOM:
		return;
	}

	if( EntityIsInPortalHole( pEntity ) )
	{
#if defined( GAME_DLL )
		if( !FindClosestPassableSpace( pEntity, m_InternalData.Placement.PortalPlane.m_Normal, pEntity->IsPlayer() ? MASK_PLAYERSOLID : MASK_SOLID ) )
		{
			if( pEntity->IsPlayer() )
			{
				CTakeDamageInfo dmgInfo( GetWorldEntity(), GetWorldEntity(), vec3_origin, vec3_origin, 1000, DMG_CRUSH );
				dmgInfo.SetDamageForce( Vector( 0, 0, -1 ) );
				dmgInfo.SetDamagePosition( pEntity->GetAbsOrigin() );
				pEntity->TakeDamage( dmgInfo );
			}
		}
#if defined( DBGFLAG_ASSERT )
		else
		{
			trace_t trAssert;
			UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), pEntity->IsPlayer() ? MASK_PLAYERSOLID : MASK_SOLID, pEntity, pEntity->GetCollisionGroup(), &trAssert );
			Assert( !trAssert.startsolid );
		}
#endif
#else
		FindClosestPassableSpace( pEntity, m_InternalData.Placement.PortalPlane.m_Normal );
#endif
	}
}

extern ConVar sv_portal_new_player_trace;

RayInPortalHoleResult_t CPortalSimulator::IsRayInPortalHole( const Ray_t &ray ) const
{
	AssertMsg( m_InternalData.Placement.pHoleShapeCollideable, "Portal wasn't set up properly." );
	if( m_InternalData.Placement.pHoleShapeCollideable == NULL ) //should probably catch this case higher up
		return RIPHR_NOT_TOUCHING_HOLE;

	trace_t Trace;
	UTIL_ClearTrace( Trace );
	physcollision->TraceBox( ray, m_InternalData.Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, &Trace );

	if( sv_portal_new_player_trace.GetBool() == false )
	{
		return Trace.DidHit() ? RIPHR_TOUCHING_HOLE_NOT_WALL : RIPHR_NOT_TOUCHING_HOLE;
	}

	if( Trace.DidHit() )
	{
		if( m_InternalData.Placement.pInvHoleShapeCollideable == NULL )
			return RIPHR_TOUCHING_HOLE_NOT_WALL;

		trace_t TraceInv;
		UTIL_ClearTrace( TraceInv );
		physcollision->TraceBox( ray, m_InternalData.Placement.pInvHoleShapeCollideable, vec3_origin, vec3_angle, &TraceInv );
		if( ray.m_IsSwept )
		{
			//we get a little funky when handling a swept ray
			//There are two distinct cases to consider, rays originating in the portal hole and rays travelling into the portal hole
			//
			if( TraceInv.DidHit() )
			{
				//if originating entirely from within the portal, we'll call this a portal-only touch
				return (Trace.startsolid && !TraceInv.startsolid) ? RIPHR_TOUCHING_HOLE_NOT_WALL : RIPHR_TOUCHING_HOLE_AND_WALL;				
			}
			else
			{
				return RIPHR_TOUCHING_HOLE_NOT_WALL;
			}
		}

		return TraceInv.DidHit() ? RIPHR_TOUCHING_HOLE_AND_WALL : RIPHR_TOUCHING_HOLE_NOT_WALL;
	}
	else
	{
		return RIPHR_NOT_TOUCHING_HOLE;
	}
}

static inline void SetupEntityPortalHoleCarvePlanes( PS_PlacementData_t &PlacementData, VMatrix &matTransform, float fClip_Front[4], float fClip_BackTop[2][4], float fClip_BackBottom[2][4], float fClip_BackLeft[4][4], float fClip_BackRight[4][4] )
{
	const float fHalfHoleWidth = PlacementData.fHalfWidth + PORTAL_HOLE_HALF_WIDTH_MOD + PORTAL_WALL_MIN_THICKNESS;
	const float fHalfHoleHeight = PlacementData.fHalfHeight + PORTAL_HOLE_HALF_HEIGHT_MOD + PORTAL_WALL_MIN_THICKNESS;

	Vector vTransformedForward = matTransform.ApplyRotation( PlacementData.vForward );
	Vector vTransformedRight = matTransform.ApplyRotation( PlacementData.vRight );
	Vector vTransformedUp = matTransform.ApplyRotation( PlacementData.vUp );
	Vector vTransformedCenter = matTransform * PlacementData.ptCenter;
	Vector vTransformedDown = -vTransformedUp;
	Vector vTransformedLeft = -vTransformedRight;

	//forward reverse conventions signify whether the normal is the same direction as m_InternalData.Placement.PortalPlane.m_Normal
	float fClipPlane_Forward[4] = {	vTransformedForward.x,
									vTransformedForward.y,
									vTransformedForward.z,
									vTransformedForward.Dot( vTransformedCenter ) + PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT };

	//fClipPlane_Front is the negated version of fClipPlane_Forward
	fClip_Front[0] = -fClipPlane_Forward[0];
	fClip_Front[1] = -fClipPlane_Forward[1];
	fClip_Front[2] = -fClipPlane_Forward[2];
	fClip_Front[3] = -fClipPlane_Forward[3];
	
	memcpy( &fClip_BackTop[0][0], &fClipPlane_Forward[0], sizeof( float ) * 4 );
	memcpy( &fClip_BackTop[1][0], &vTransformedDown.x, sizeof( float ) * 3 );
	fClip_BackTop[1][3] = vTransformedDown.Dot( vTransformedCenter + (vTransformedUp * fHalfHoleHeight) );

	memcpy( &fClip_BackBottom[0][0], &fClipPlane_Forward[0], sizeof( float ) * 4 );
	memcpy( &fClip_BackBottom[1][0], &vTransformedUp.x, sizeof( float ) * 3 );
	fClip_BackBottom[1][3] = vTransformedUp.Dot( vTransformedCenter + (vTransformedDown * fHalfHoleHeight) );

	memcpy( &fClip_BackLeft[0][0], &fClip_BackBottom[0][0], sizeof( float ) * 7 );
	fClip_BackLeft[1][3] = vTransformedUp.Dot( vTransformedCenter + (vTransformedUp * fHalfHoleHeight) );
	memcpy( &fClip_BackLeft[2][0], &vTransformedDown.x, sizeof( float ) * 3 );
	fClip_BackLeft[2][3] = vTransformedDown.Dot( vTransformedCenter + (vTransformedDown * fHalfHoleHeight) );
	memcpy( &fClip_BackLeft[3][0], &vTransformedRight.x, sizeof( float ) * 3 );
	fClip_BackLeft[3][3] = vTransformedRight.Dot( vTransformedCenter + (vTransformedLeft * fHalfHoleWidth) );

	memcpy( &fClip_BackRight[0][0], &fClip_BackLeft[0][0], sizeof( float ) * 12 );
	memcpy( &fClip_BackRight[3][0], &vTransformedLeft.x, sizeof( float ) * 3 );
	fClip_BackRight[3][3] = vTransformedLeft.Dot( vTransformedCenter + (vTransformedRight * fHalfHoleWidth) );
}

static void CarveEntity( PS_PlacementData_t &PlacementData, PS_SD_Dynamic_CarvedEntities_t &CarvedEntities, PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &CarvedRepresentation )
{
	Assert( CarvedRepresentation.pSourceEntity != NULL );
	Assert( CarvedRepresentation.pCollide == NULL );

	//create the polyhedrons and collideables
	ICollideable *pProp = CarvedRepresentation.pSourceEntity->GetCollideable();
	VMatrix matCollisionToWorld( pProp->CollisionToWorldTransform() );
	VMatrix matWorldToCollision;
	MatrixInverseTR( matCollisionToWorld, matWorldToCollision );


	SolidType_t solidType = CarvedRepresentation.pSourceEntity->GetSolid();
	if( solidType == SOLID_VPHYSICS )
	{
		vcollide_t *pCollide = modelinfo->GetVCollide( pProp->GetCollisionModelIndex() );
		Assert( pCollide != NULL );
		if( pCollide != NULL )
		{
			CPhysConvex *ConvexesArray[1024];
			int iConvexCount = 0;
			for( int i = 0; i != pCollide->solidCount; ++i )
			{
				iConvexCount += physcollision->GetConvexesUsedInCollideable( pCollide->solids[i], ConvexesArray, 1024 - iConvexCount );
			}

			CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex = CarvedEntities.Polyhedrons.Count();
			for( int i = 0; i != iConvexCount; ++i )
			{
				CPolyhedron *pFullPolyhedron = physcollision->PolyhedronFromConvex( ConvexesArray[i], false );
				if( pFullPolyhedron != NULL )
				{
					CarvedEntities.Polyhedrons.AddToTail( pFullPolyhedron );
				}
			}

			CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons = CarvedEntities.Polyhedrons.Count() - CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex;
		}
	}
	else if( solidType == SOLID_BSP )
	{
		CBrushQuery brushQuery;
		//enginetrace->GetBrushesInAABB( vAABBMins, vAABBMaxs, WorldBrushes, MASK_SOLID_BRUSHONLY|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP );
		enginetrace->GetBrushesInCollideable( pProp, brushQuery );

		//create locally clipped polyhedrons for the world
		{
			CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex = CarvedEntities.Polyhedrons.Count();
			uint32 *pBrushList = brushQuery.Base();
			int iBrushCount = brushQuery.Count();
			ConvertBrushListToClippedPolyhedronList( pBrushList, iBrushCount, NULL, 0, PORTAL_POLYHEDRON_CUT_EPSILON, &CarvedEntities.Polyhedrons );
			CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons = CarvedEntities.Polyhedrons.Count() - CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex;
		}
	}

	CPolyhedron **pPolyhedrons = (CPolyhedron **)stackalloc( sizeof( CPolyhedron * ) * CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons * 5 ); //*5 for front, back left, back right, back top, back bottom. 
	int iPolyhedronCount = 0;

	float fClip_Front[4];
	float fClip_BackTop[2][4];
	float fClip_BackBottom[2][4];
	float fClip_BackLeft[4][4];
	float fClip_BackRight[4][4];
	SetupEntityPortalHoleCarvePlanes( PlacementData, matWorldToCollision, fClip_Front, fClip_BackTop, fClip_BackBottom, fClip_BackLeft, fClip_BackRight );

	for( int i = 0; i != CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons; ++i )
	{
		CPolyhedron *pUncarvedPolyhedron = CarvedEntities.Polyhedrons[CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex + i];
		CPolyhedron *pCarvedPolyhedron;

		//clip to in front of the plane, single piece
		pCarvedPolyhedron = ClipPolyhedron( pUncarvedPolyhedron, (float *)fClip_Front, 1, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pCarvedPolyhedron != NULL )
		{
			pPolyhedrons[iPolyhedronCount++] = pCarvedPolyhedron;
		}

		//4 carves behind the plane to form the pieces around the hole
		pCarvedPolyhedron = ClipPolyhedron( pUncarvedPolyhedron, (float *)fClip_BackTop, 2, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pCarvedPolyhedron != NULL )
		{
			pPolyhedrons[iPolyhedronCount++] = pCarvedPolyhedron;
		}

		pCarvedPolyhedron = ClipPolyhedron( pUncarvedPolyhedron, (float *)fClip_BackBottom, 2, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pCarvedPolyhedron != NULL )
		{
			pPolyhedrons[iPolyhedronCount++] = pCarvedPolyhedron;
		}

		pCarvedPolyhedron = ClipPolyhedron( pUncarvedPolyhedron, (float *)fClip_BackLeft, 4, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pCarvedPolyhedron != NULL )
		{
			pPolyhedrons[iPolyhedronCount++] = pCarvedPolyhedron;
		}

		pCarvedPolyhedron = ClipPolyhedron( pUncarvedPolyhedron, (float *)fClip_BackRight, 4, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pCarvedPolyhedron != NULL )
		{
			pPolyhedrons[iPolyhedronCount++] = pCarvedPolyhedron;
		}
	}

	CarvedRepresentation.CarvedPolyhedronGroup.iStartIndex = CarvedEntities.Polyhedrons.Count();
	if( iPolyhedronCount != 0 )
	{
		CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons = iPolyhedronCount;
		CarvedEntities.Polyhedrons.AddMultipleToTail( iPolyhedronCount, pPolyhedrons );
	}
	else
	{
		CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons = 0;
	}
}

static void DestroyCollideable( CPhysCollide **ppCollide )
{
	if ( *ppCollide )
	{
#if defined( GAME_DLL )
		physenv->DestroyCollideOnDeadObjectFlush( *ppCollide );
#else
		physcollision->DestroyCollide( *ppCollide );
#endif
		*ppCollide = NULL;
	}
}


void CPortalSimulator::AddCarvedEntity( CBaseEntity *pEntity )
{
	PS_SD_Dynamic_CarvedEntities_t &CarvedEntities = m_InternalData.Simulation.Dynamic.CarvedEntities;

	//make sure it's not already in the list
	int iCarvedEntityCount = CarvedEntities.CarvedRepresentations.Count();
	for( int i = 0; i != iCarvedEntityCount; ++i )
	{
		if( CarvedEntities.CarvedRepresentations[i].pSourceEntity == pEntity )
		{
			Assert( IsEntityCarvedByPortal( pEntity->entindex() ) );
			return;
		}
	}

	Assert( !IsEntityCarvedByPortal( pEntity->entindex() ) );

	int iEntIndex = pEntity->entindex();
	int iArrayIndex = iEntIndex / 32;
	m_InternalData.Simulation.Dynamic.HasCarvedVersionOfEntity[iArrayIndex] |= (1 << (iEntIndex - (iArrayIndex * 32)));

	PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &CarvedRepresentation = CarvedEntities.CarvedRepresentations[CarvedEntities.CarvedRepresentations.AddToTail()];
	CarvedRepresentation.pSourceEntity = pEntity;
	CarvedRepresentation.pCollide = NULL;
#ifndef CLIENT_DLL
	CarvedRepresentation.pPhysicsObject = NULL;
#endif
	CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex = 0;
	CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons = 0;
	CarvedRepresentation.CarvedPolyhedronGroup.iStartIndex = 0;
	CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons = 0;

#ifndef CLIENT_DLL
	//we don't clone entities that we carve
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_CLONES_ENTITY_FROM_MAIN;
#endif

	convertconvexparams_t params;
	params.Defaults();
	params.buildOptimizedTraceTables = true;
	params.bUseFastApproximateInertiaTensor = true;
	//some immediate setup may be required
	if( IsCollisionGenerationEnabled() && m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists )
	{
		CarveEntity( m_InternalData.Placement, CarvedEntities, CarvedRepresentation );

		if( CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons != 0 )
		{
			CPolyhedron **ppPolyhedrons = CarvedEntities.Polyhedrons.Base() + CarvedRepresentation.CarvedPolyhedronGroup.iStartIndex;
			CPhysConvex **pCarvedConvexes = (CPhysConvex **)stackalloc( sizeof( CPhysConvex * ) * CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons );

			for( int i = 0; i != CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons; ++i )
			{
				pCarvedConvexes[i] = physcollision->ConvexFromConvexPolyhedron( *ppPolyhedrons[i] );
				Assert( pCarvedConvexes[i] != NULL );
			}

			CarvedRepresentation.pCollide = physcollision->ConvertConvexToCollideParams( pCarvedConvexes, CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons, params );

			Assert( CarvedRepresentation.pCollide != NULL );

#ifndef CLIENT_DLL
			if( CarvedRepresentation.pCollide && IsSimulatingVPhysics() && m_InternalData.Simulation.Dynamic.CarvedEntities.bPhysicsExists )
			{
				ICollideable *pProp = CarvedRepresentation.pSourceEntity->GetCollideable();

				// Create the physics object
				objectparams_t params = g_PhysDefaultObjectParams;
				params.pGameData = m_InternalData.Simulation.hCollisionEntity;

				//add to the collision entity
				//CarvedRepresentation.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObject( CarvedRepresentation.pCollide, physprops->GetSurfaceIndex( "default" ), pProp->GetCollisionOrigin(), pProp->GetCollisionAngles(), &params );
				CarvedRepresentation.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( CarvedRepresentation.pCollide, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, pProp->GetCollisionOrigin(), pProp->GetCollisionAngles(), &params );
			}
#endif
		}
	}
}

void CPortalSimulator::ReleaseCarvedEntity( CBaseEntity *pEntity )
{
	PS_SD_Dynamic_CarvedEntities_t &CarvedEntities = m_InternalData.Simulation.Dynamic.CarvedEntities;

	if( !IsEntityCarvedByPortal( pEntity->entindex() ) )
		return;

	int iCarvedEntityCount = CarvedEntities.CarvedRepresentations.Count();
	for( int i = 0; i != iCarvedEntityCount; ++i )
	{
		if( CarvedEntities.CarvedRepresentations[i].pSourceEntity == pEntity )
		{
			//found it, kill it
			PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &CarvedRepresentation = CarvedEntities.CarvedRepresentations[i];

#ifndef CLIENT_DLL
			if( CarvedRepresentation.pPhysicsObject != NULL )
			{
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( CarvedRepresentation.pPhysicsObject );
				CarvedRepresentation.pPhysicsObject = NULL;
			}
#endif

			DestroyCollideable( &CarvedRepresentation.pCollide );

			if( (CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons != 0) || (CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons != 0) )
			{
				int iStart = CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex;
				Assert( (CarvedRepresentation.UncarvedPolyhedronGroup.iStartIndex + CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons) == CarvedRepresentation.CarvedPolyhedronGroup.iStartIndex ); //We assume the groups are back to back
				int iPolyhedronCount = CarvedRepresentation.UncarvedPolyhedronGroup.iNumPolyhedrons + CarvedRepresentation.CarvedPolyhedronGroup.iNumPolyhedrons;

				for( int j = 0; j != iPolyhedronCount; ++j )
				{
					CarvedEntities.Polyhedrons[j + iStart]->Release();
				}
				CarvedEntities.Polyhedrons.RemoveMultiple( iStart, iPolyhedronCount );
				for( int j = 0; j != iCarvedEntityCount; ++j )
				{
					//shift every polyhedron group's start index to cover up the hole we just made. This invalidates our own start indices
					CarvedEntities.CarvedRepresentations[j].UncarvedPolyhedronGroup.iStartIndex -= iPolyhedronCount;
					CarvedEntities.CarvedRepresentations[j].CarvedPolyhedronGroup.iStartIndex -= iPolyhedronCount;
				}
			}

			CarvedEntities.CarvedRepresentations.FastRemove( i );
			int iEntIndex = pEntity->entindex();
			int iArrayIndex = iEntIndex / 32;
			m_InternalData.Simulation.Dynamic.HasCarvedVersionOfEntity[iArrayIndex] &= ~(1 << (iEntIndex - (iArrayIndex * 32)));

#ifndef CLIENT_DLL
			if( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.Find( pEntity ) != m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.InvalidIndex() )
			{
				//re-enabled cloning since we've stopped carving
				m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_CLONES_ENTITY_FROM_MAIN;
			}
#endif
			break;
		}
	}
}

bool CPortalSimulator::IsEntityCarvedByPortal( int iEntIndex ) const
{
	if( iEntIndex < 0 )
		return false;

	Assert( (iEntIndex >= 0) && (iEntIndex < MAX_EDICTS) );
	int iArrayIndex = iEntIndex / 32;
	return (m_InternalData.Simulation.Dynamic.HasCarvedVersionOfEntity[iArrayIndex] & (1 << (iEntIndex - (iArrayIndex * 32)))) != 0;
}


CPhysCollide *CPortalSimulator::GetCollideForCarvedEntity( CBaseEntity *pEntity ) const
{
	Assert( pEntity != NULL );
	if( (m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists == false) || (m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count() == 0) )
		return NULL;

	CUtlVector<PS_SD_Dynamic_CarvedEntities_CarvedEntity_t> const &CarvedRepresentations = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations;
	int iCarvedEntityCount = CarvedRepresentations.Count();
	const PS_SD_Dynamic_CarvedEntities_CarvedEntity_t * pCarvedEntities = CarvedRepresentations.Base();
	for( int i = 0; i != iCarvedEntityCount; ++i )
	{
		if( pCarvedEntities[i].pSourceEntity == pEntity )
		{
			return pCarvedEntities[i].pCollide;
		}
	}

	return NULL;
}


void CPortalSimulator::SetCarvedParent( CBaseEntity *pPortalPlacementParent )
{
	CBaseEntity *pExistingParent = m_InternalData.Placement.hPortalPlacementParent.Get();

	if( pPortalPlacementParent == pExistingParent )
		return;

	m_InternalData.Placement.hPortalPlacementParent = pPortalPlacementParent;
	
	if( pExistingParent != NULL )
	{
		ReleaseCarvedEntity( pExistingParent );
	}

	if( pPortalPlacementParent )
	{
		AddCarvedEntity( pPortalPlacementParent );
	}

	bool bOldIsShrunk = m_InternalData.Placement.bParentIsVPhysicsSolidBrush;
	
	if( pPortalPlacementParent && (pPortalPlacementParent->GetSolid() == SOLID_VPHYSICS) )
	{
		const model_t *pModel = pPortalPlacementParent->GetModel();
		m_InternalData.Placement.bParentIsVPhysicsSolidBrush = pModel && ((modtype_t)modelinfo->GetModelType( pModel ) == mod_brush);
	}
	else
	{
		m_InternalData.Placement.bParentIsVPhysicsSolidBrush = false;
	}
	

	
#if 1
	//if the entity is a brush model using SOLID_VPHYSICS then it's model is actually half an inch smaller than the brushes on all sides! See usage of VPHYSICS_SHRINK in utils\vbsp\ivp.cpp
	//TODO: instead of assuming this shrinkage exists, encode it in the map somehow. We can't just blindly go with the brush geometry because the collision will be wrong, and we can't
	//		blindly go with the collision geometry because the portal will be behind the rendering surface of the brush. Need to be actively aware of the discrepancy.
	{		
		if( bOldIsShrunk != m_InternalData.Placement.bParentIsVPhysicsSolidBrush )
		{
			//recarve the tube collideable

			for( int i = 0; i != m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count(); ++i )
			{
				m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons[i]->Release();
			}
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.RemoveAll();

#if defined( GAME_DLL )
			bool bHadPhysObject = false;
			bool bWasCollisionEntPhys = false;
			if( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject != NULL )
			{
				bHadPhysObject = true;
				if( m_InternalData.Simulation.hCollisionEntity && 
					(m_InternalData.Simulation.hCollisionEntity->VPhysicsGetObject() == m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject) )
				{
					bWasCollisionEntPhys = true;
					m_InternalData.Simulation.hCollisionEntity->VPhysicsSetObject( NULL );
				}
				
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject );
				m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject = NULL;				
			}
#endif

			CreateTubePolyhedrons();

			if( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable != NULL )
			{
				DestroyCollideable( &m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable );
				
				if( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() != 0 )
				{
					m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Base(), m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() );
				}
			}

#if defined( GAME_DLL )
			if( bHadPhysObject )
			{
				if( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable != NULL )
				{
					//int iDefaultSurfaceIndex = physprops->GetSurfaceIndex( "default" );
					objectparams_t params = g_PhysDefaultObjectParams;

					// Any non-moving object can point to world safely-- Make sure we dont use 'params' for something other than that beyond this point.
					if( m_InternalData.Simulation.hCollisionEntity )
					{
						params.pGameData = m_InternalData.Simulation.hCollisionEntity;
					}
					else
					{
						params.pGameData = GetWorldEntity();
					}

					m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );

					if( bWasCollisionEntPhys )
					{
						m_InternalData.Simulation.hCollisionEntity->VPhysicsSetObject(m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject);
					}

					m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
				}
			}
#endif
		}
	}
#endif

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS )
	if( sv_dump_portalsimulator_collision.GetBool() )
	{
		const char *szFileName = "pscd_" s_szDLLName "_carvedparent.txt";
		filesystem->RemoveFile( szFileName );
		DumpActiveCollision( this, szFileName );
		if( m_pLinkedPortal )
		{
			szFileName = "pscd_" s_szDLLName "_linked_carvedparent.txt";
			filesystem->RemoveFile( szFileName );
			DumpActiveCollision( m_pLinkedPortal, szFileName );
		}
	}
#endif
}


void CPortalSimulator::ClearEverything( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::Clear() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();	

#ifndef CLIENT_DLL
	ClearAllPhysics();
#endif
	ClearAllCollision();
	ClearPolyhedrons();

	ReleaseAllEntityOwnership();

#ifndef CLIENT_DLL
	Assert( (m_InternalData.Simulation.hCollisionEntity == NULL) || OwnsEntity(m_InternalData.Simulation.hCollisionEntity) );
#endif

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::Clear() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::AttachTo( CPortalSimulator *pLinkedPortalSimulator )
{
	Assert( pLinkedPortalSimulator );

	if( pLinkedPortalSimulator == m_pLinkedPortal )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::AttachTo() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	DetachFromLinked();

	m_pLinkedPortal = pLinkedPortalSimulator;
	pLinkedPortalSimulator->m_pLinkedPortal = this;

	if( m_bLocalDataIsReady && m_pLinkedPortal->m_bLocalDataIsReady )
	{
		UpdateLinkMatrix();
		CreateLinkedCollision();
#ifndef CLIENT_DLL
		CreateLinkedPhysics();
#endif
	}

#if defined( DEBUG_PORTAL_COLLISION_ENVIRONMENTS )
	if( sv_dump_portalsimulator_collision.GetBool() )
	{
		const char *szFileName = "pscd_" s_szDLLName ".txt";
		filesystem->RemoveFile( szFileName );
		DumpActiveCollision( this, szFileName );
		if( m_pLinkedPortal )
		{
			szFileName = "pscd_" s_szDLLName "_linked.txt";
			filesystem->RemoveFile( szFileName );
			DumpActiveCollision( m_pLinkedPortal, szFileName );
		}
	}
#endif

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::AttachTo() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}


void CPortalSimulator::TakeOwnershipOfEntity( CBaseEntity *pEntity )
{
	AssertMsg( m_bLocalDataIsReady, "Tell the portal simulator where it is with MoveTo() before using it in any other way." );

	Assert( pEntity != NULL );
	if( pEntity == NULL )
		return;

	if( pEntity->entindex() < 0 )
		return;

	if( pEntity->IsWorld() )
		return;
#if defined( GAME_DLL )
	if( CPhysicsShadowClone::IsShadowClone( pEntity ) )
		return;

	if( pEntity->GetServerVehicle() != NULL ) //we don't take kindly to vehicles in these here parts. Their physics controllers currently don't migrate properly and cause a crash
		return;
#endif

	if( OwnsEntity( pEntity ) )
		return;

	Assert( GetSimulatorThatOwnsEntity( pEntity ) == NULL );
	MarkAsOwned( pEntity );
	Assert( GetSimulatorThatOwnsEntity( pEntity ) == this );

	if( EntityIsInPortalHole( pEntity ) )
		m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] |= PSEF_IS_IN_PORTAL_HOLE;
	else
		m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_IS_IN_PORTAL_HOLE;

#if defined( GAME_DLL )
	UpdateShadowClonesPortalSimulationFlags( pEntity, PSEF_IS_IN_PORTAL_HOLE, m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] );
#endif

	m_pCallbacks->PortalSimulator_TookOwnershipOfEntity( pEntity );

#if defined( GAME_DLL )
	if( IsSimulatingVPhysics() )
		TakePhysicsOwnership( pEntity );
#endif

	pEntity->CollisionRulesChanged(); //absolutely necessary in single-environment mode, possibly expendable in multi-environment moder
	//pEntity->SetGroundEntity( NULL );
	IPhysicsObject *pObject = pEntity->VPhysicsGetObject();
	if( pObject )
	{
		pObject->Wake();
		pObject->RecheckContactPoints();
	}

	CUtlVector<CBaseEntity *> childrenList;
	GetAllChildren( pEntity, childrenList );
	for ( int i = childrenList.Count(); --i >= 0; )
	{
		CBaseEntity *pEnt = childrenList[i];
		CPortalSimulator *pOwningSimulator = GetSimulatorThatOwnsEntity( pEnt );
		if( pOwningSimulator != this )
		{
			if( pOwningSimulator != NULL )
				pOwningSimulator->ReleaseOwnershipOfEntity( pEnt, (pOwningSimulator == m_pLinkedPortal) );

			TakeOwnershipOfEntity( childrenList[i] );
		}
	}
}

void RecheckEntityCollision( CBaseEntity *pEntity )
{
	CCallQueue *pCallQueue;
	if ( (pCallQueue = GetPortalCallQueue()) != NULL )
	{
		pCallQueue->QueueCall( RecheckEntityCollision, pEntity );
		return;
	}

	pEntity->CollisionRulesChanged(); //absolutely necessary in single-environment mode, possibly expendable in multi-environment mode
	//pEntity->SetGroundEntity( NULL );
	IPhysicsObject *pObject = pEntity->VPhysicsGetObject();
	if( pObject )
	{
		pObject->Wake();
		pObject->RecheckContactPoints();
	}
}

void CPortalSimulator::ReleaseOwnershipOfEntity( CBaseEntity *pEntity, bool bMovingToLinkedSimulator /*= false*/ )
{
	if( pEntity == NULL )
		return;

	if( pEntity->IsWorld() )
		return;

	if( !OwnsEntity( pEntity ) )
		return;

#if defined( GAME_DLL )
	if( m_InternalData.Simulation.pPhysicsEnvironment )
		ReleasePhysicsOwnership( pEntity, true, bMovingToLinkedSimulator );
#endif

	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_IS_IN_PORTAL_HOLE;

#if defined( GAME_DLL )
	UpdateShadowClonesPortalSimulationFlags( pEntity, PSEF_IS_IN_PORTAL_HOLE, m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] );
#endif

	Assert( GetSimulatorThatOwnsEntity( pEntity ) == this );
	MarkAsReleased( pEntity );
	Assert( GetSimulatorThatOwnsEntity( pEntity ) == NULL );

	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Dynamic.OwnedEntities[i] == pEntity )
		{
			m_InternalData.Simulation.Dynamic.OwnedEntities.FastRemove(i);
			break;
		}
	}

	if( bMovingToLinkedSimulator == false )
	{
		RecheckEntityCollision( pEntity );
	}

	m_pCallbacks->PortalSimulator_ReleasedOwnershipOfEntity( pEntity );

	CUtlVector<CBaseEntity *> childrenList;
	GetAllChildren( pEntity, childrenList );
	for ( int i = childrenList.Count(); --i >= 0; )
		ReleaseOwnershipOfEntity( childrenList[i], bMovingToLinkedSimulator );
}

void CPortalSimulator::ReleaseAllEntityOwnership( void )
{
	//Assert( m_bLocalDataIsReady || (m_InternalData.Simulation.Dynamic.OwnedEntities.Count() == 0) );
	int iSkippedObjects = 0;
	while( m_InternalData.Simulation.Dynamic.OwnedEntities.Count() != iSkippedObjects ) //the release function changes OwnedEntities
	{
		CBaseEntity *pEntity = m_InternalData.Simulation.Dynamic.OwnedEntities[iSkippedObjects];

#if defined( GAME_DLL )
		if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity )
			|| CPhysicsShadowClone::IsShadowClone( pEntity ) )
		{
			++iSkippedObjects;
			continue;
		}
#endif
		RemoveEntityFromPortalHole( pEntity ); //assume that whenever someone wants to release all entities, it's because the portal is going away
		ReleaseOwnershipOfEntity( pEntity );
	}

#if defined( GAME_DLL )
	//HACK: should probably separate out these releases of cloned objects. But the calling pattern is identical to outside code for now and the leafy bits are a bit too leafy for this late of a change
	int iReleaseClonedEnts = m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.Count();
	if( iReleaseClonedEnts != 0 )
	{
		CBaseEntity **pReleaseEnts = (CBaseEntity **)stackalloc( sizeof( CBaseEntity * ) * iReleaseClonedEnts );
		memcpy( pReleaseEnts, m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.Base(), sizeof( CBaseEntity * ) * iReleaseClonedEnts );

		for( int i = iReleaseClonedEnts; --i >= 0; )
		{
			StopCloningEntityAcrossPortals( pReleaseEnts[i] );
		}
	}
	Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.Count() == 0 );

	Assert( (m_InternalData.Simulation.hCollisionEntity == NULL) || OwnsEntity(m_InternalData.Simulation.hCollisionEntity) );
#endif
}


void CPortalSimulator::MarkAsOwned( CBaseEntity *pEntity )
{
	Assert( pEntity != NULL );
	int iEntIndex = pEntity->entindex();
	Assert( s_OwnedEntityMap[iEntIndex] == NULL );
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
		Assert( m_InternalData.Simulation.Dynamic.OwnedEntities[i] != pEntity );
#endif
	Assert( (m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] & PSEF_OWNS_ENTITY) == 0 );

	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_OWNS_ENTITY;
	s_OwnedEntityMap[iEntIndex] = this;
	m_InternalData.Simulation.Dynamic.OwnedEntities.AddToTail( pEntity );
}

void CPortalSimulator::MarkAsReleased( CBaseEntity *pEntity )
{
	Assert( pEntity != NULL );
	int iEntIndex = pEntity->entindex();
	Assert( s_OwnedEntityMap[iEntIndex] == this );
#if defined( GAME_DLL )
	Assert( ((m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] & PSEF_OWNS_ENTITY) != 0) || CPSCollisionEntity::IsPortalSimulatorCollisionEntity(pEntity) );
#else
	Assert( (m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] & PSEF_OWNS_ENTITY) != 0 );
#endif

	s_OwnedEntityMap[iEntIndex] = NULL;
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_OWNS_ENTITY;
	int i;
	for( i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Dynamic.OwnedEntities[i] == pEntity )
		{
			m_InternalData.Simulation.Dynamic.OwnedEntities.FastRemove(i);
			break;
		}
	}
	Assert( i >= 0 );
}

#ifndef CLIENT_DLL
void CPortalSimulator::TakePhysicsOwnership( CBaseEntity *pEntity )
{
	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	Assert( CPhysicsShadowClone::IsShadowClone( pEntity ) == false );
	Assert( OwnsEntity( pEntity ) ); //taking physics ownership happens AFTER general ownership
	
	if( OwnsPhysicsForEntity( pEntity ) )
		return;

	int iEntIndex = pEntity->entindex();
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_OWNS_PHYSICS;


	//physics cloning
	{	
#ifdef _DEBUG
		{
			int iDebugIndex;
			for( iDebugIndex = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --iDebugIndex >= 0; )
			{
				if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[iDebugIndex]->GetClonedEntity() == pEntity )
					break;
			}
			AssertMsg( iDebugIndex < 0, "Trying to own an entity, when a clone from the linked portal already exists" ); 

			if( m_pLinkedPortal )
			{
				for( iDebugIndex = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --iDebugIndex >= 0; )
				{
					if( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[iDebugIndex]->GetClonedEntity() == pEntity )
						break;
				}
				AssertMsg( iDebugIndex < 0, "Trying to own an entity, when we're already exporting a clone to the linked portal" );
			}

			//Don't require a copy from main to already exist
		}
#endif

		EHANDLE hEnt = pEntity;

		//To linked portal
		if( m_pLinkedPortal && m_pLinkedPortal->m_InternalData.Simulation.pPhysicsEnvironment )
		{

			DBG_CODE(
				for( int i = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
					AssertMsg( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i]->GetClonedEntity() != pEntity, "Already cloning to linked portal." );
			);

			CPhysicsShadowClone *pClone = CPhysicsShadowClone::CreateShadowClone( m_pLinkedPortal->m_InternalData.Simulation.pPhysicsEnvironment, hEnt, "CPortalSimulator::TakePhysicsOwnership(): To Linked Portal", &m_InternalData.Placement.matThisToLinked.As3x4() );
			if( pClone )
			{
				//bool bHeldByPhyscannon = false;
				CBaseEntity *pHeldEntity = NULL;
				CPortal_Player *pPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pEntity );

				if ( !pPlayer && pEntity->IsPlayer() )
				{
					pPlayer = (CPortal_Player *)pEntity;
				}

				if ( pPlayer && !pPlayer->IsUsingVMGrab() )
				{
					pHeldEntity = GetPlayerHeldEntity( pPlayer );
					/*if ( !pHeldEntity )
					{
						pHeldEntity = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
						bHeldByPhyscannon = true;
					}*/

					if( pHeldEntity )
					{
						//player is holding the entity, force them to pick it back up again
						bool bIsHeldObjectOnOppositeSideOfPortal = pPlayer->IsHeldObjectOnOppositeSideOfPortal();
						pPlayer->m_bSilentDropAndPickup = true;
						pPlayer->ForceDropOfCarriedPhysObjects( pHeldEntity );
						pPlayer->SetHeldObjectOnOppositeSideOfPortal( bIsHeldObjectOnOppositeSideOfPortal );
					}
				}

				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.AddToTail( pClone );
				m_pLinkedPortal->MarkAsOwned( pClone );
				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= PSEF_OWNS_PHYSICS;
				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_IS_IN_PORTAL_HOLE;
				pClone->CollisionRulesChanged(); //adding the clone to the portal simulator changes how it collides

				if( pHeldEntity )
				{
					/*if ( bHeldByPhyscannon )
					{
						PhysCannonPickupObject( pPlayer, pHeldEntity );
					}
					else*/
					{
						PlayerPickupObject( pPlayer, pHeldEntity );
					}
					pPlayer->m_bSilentDropAndPickup = false;
				}
			}
		}
	}

	m_pCallbacks->PortalSimulator_TookPhysicsOwnershipOfEntity( pEntity );
}


void CPortalSimulator::ReleasePhysicsOwnership( CBaseEntity *pEntity, bool bContinuePhysicsCloning /*= true*/, bool bMovingToLinkedSimulator /*= false*/ )
{
	if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	Assert( OwnsEntity( pEntity ) ); //releasing physics ownership happens BEFORE releasing general ownership
	Assert( CPhysicsShadowClone::IsShadowClone( pEntity ) == false );

	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	if( !OwnsPhysicsForEntity( pEntity ) )
		return;

	if( IsSimulatingVPhysics() == false )
		bContinuePhysicsCloning = false;

	int iEntIndex = pEntity->entindex();
	m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_OWNS_PHYSICS;
	
	//physics cloning
	{
#ifdef _DEBUG
		{
			int iDebugIndex;
			for( iDebugIndex = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --iDebugIndex >= 0; )
			{
				if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[iDebugIndex]->GetClonedEntity() == pEntity )
					break;
			}
			AssertMsg( iDebugIndex < 0, "Trying to release an entity, when a clone from the linked portal already exists." );
		}
#endif

		//clear exported clones
		{
			DBG_CODE_NOSCOPE( bool bFoundAlready = false; );
			DBG_CODE_NOSCOPE( const char *szLastFoundMarker = NULL; );

			//to linked portal
			if( m_pLinkedPortal )
			{
				DBG_CODE_NOSCOPE( bFoundAlready = false; );
				for( int i = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
				{
					if( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i]->GetClonedEntity() == pEntity )
					{
						CPhysicsShadowClone *pClone = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
						AssertMsg( bFoundAlready == false, "Multiple clones to linked portal found." );
						DBG_CODE_NOSCOPE( bFoundAlready = true; );
						DBG_CODE_NOSCOPE( szLastFoundMarker = pClone->m_szDebugMarker );

						//bool bHeldByPhyscannon = false;
						CBaseEntity *pHeldEntity = NULL;
						CPortal_Player *pPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pEntity );

						if ( !pPlayer && pEntity->IsPlayer() )
						{
							pPlayer = (CPortal_Player *)pEntity;
						}

						if ( pPlayer && !pPlayer->IsUsingVMGrab() )
						{
							pHeldEntity = GetPlayerHeldEntity( pPlayer );

							/*if ( !pHeldEntity )
							{
								pHeldEntity = PhysCannonGetHeldEntity( pPlayer->GetActiveWeapon() );
								bHeldByPhyscannon = true;
							}*/

							if( pHeldEntity )
							{
								//player is holding the entity, force them to pick it back up again
								bool bIsHeldObjectOnOppositeSideOfPortal = pPlayer->IsHeldObjectOnOppositeSideOfPortal();
								pPlayer->m_bSilentDropAndPickup = true;
								pPlayer->ForceDropOfCarriedPhysObjects( pHeldEntity );
								pPlayer->SetHeldObjectOnOppositeSideOfPortal( bIsHeldObjectOnOppositeSideOfPortal );
							}
							else
							{
								pHeldEntity = NULL;
							}
						}

						m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
						m_pLinkedPortal->MarkAsReleased( pClone );
						pClone->Free();
						m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.FastRemove(i);

						if( pHeldEntity )
						{
							/*if ( bHeldByPhyscannon )
							{
								PhysCannonPickupObject( pPlayer, pHeldEntity );
							}
							else*/
							{
								PlayerPickupObject( pPlayer, pHeldEntity );
							}
							pPlayer->m_bSilentDropAndPickup = false;
						}

						DBG_CODE_NOSCOPE( continue; );
						break;
					}
				}
			}
		}
	}

	m_pCallbacks->PortalSimulator_ReleasedPhysicsOwnershipOfEntity( pEntity );
}

void CPortalSimulator::StartCloningEntityFromMain( CBaseEntity *pEntity )
{
	if( CPhysicsShadowClone::IsShadowClone( pEntity ) || CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	if( (m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_CLONES_ENTITY_FROM_MAIN) != 0 )
		return; //already cloned, no work to do

#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.Count(); --i >= 0; )
		Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain[i] != pEntity );
#endif

	//NDebugOverlay::EntityBounds( pEntity, 0, 255, 0, 50, 5.0f );

	m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.AddToTail( pEntity );

	if( !IsEntityCarvedByPortal( pEntity ) )
	{
		//only set the flag to clone if we're not currently carving. We'll still hold it in the ShouldCloneFromMain list in case we stop carving it
		m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] |= PSEF_CLONES_ENTITY_FROM_MAIN;
	}

	pEntity->CollisionRulesChanged();
}

void CPortalSimulator::StopCloningEntityFromMain( CBaseEntity *pEntity )
{
	if( ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_CLONES_ENTITY_FROM_MAIN) == 0) && !IsEntityCarvedByPortal( pEntity ) )
	{
		Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.Find( pEntity ) == -1 );
		return; //not cloned, no work to do
	}

	//NDebugOverlay::EntityBounds( pEntity, 255, 0, 0, 50, 5.0f );

	m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneFromMain.FindAndFastRemove( pEntity );
	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_CLONES_ENTITY_FROM_MAIN;
	pEntity->CollisionRulesChanged();
}


void CPortalSimulator::StartCloningEntityAcrossPortals( CBaseEntity *pEntity )
{
	if( CPhysicsShadowClone::IsShadowClone( pEntity ) || CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) )
		return;

	if( (m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_CLONES_ENTITY_ACROSS_PORTAL_FROM_MAIN) != 0 )
		return; //already cloned, no work to do

#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.Count(); --i >= 0; )
		Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal[i] != pEntity );
#endif

	//NDebugOverlay::EntityBounds( pEntity, 0, 255, 0, 50, 5.0f );

	m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.AddToTail( pEntity );
	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] |= PSEF_CLONES_ENTITY_ACROSS_PORTAL_FROM_MAIN;

	//push the clone now
	if( m_pLinkedPortal && m_pLinkedPortal->m_InternalData.Simulation.pPhysicsEnvironment && m_pLinkedPortal->m_CreationChecklist.bLinkedPhysicsGenerated )
	{
		EHANDLE hEnt = pEntity;
		CPhysicsShadowClone *pClone = CPhysicsShadowClone::CreateShadowClone( m_pLinkedPortal->m_InternalData.Simulation.pPhysicsEnvironment, hEnt, "CPortalSimulator::StartCloningEntityAcrossPortals(): To Linked Portal", &m_InternalData.Placement.matThisToLinked.As3x4() );
		if( pClone )
		{
			m_pLinkedPortal->MarkAsOwned( pClone );
			m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= PSEF_OWNS_PHYSICS | (m_InternalData.Simulation.Dynamic.EntFlags[hEnt->entindex()] & PSEF_IS_IN_PORTAL_HOLE);
			m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.AddToTail( pClone );
			pClone->CollisionRulesChanged(); //adding the clone to the portal simulator changes how it collides
		}
	}
}

void CPortalSimulator::StopCloningEntityAcrossPortals( CBaseEntity *pEntity )
{
	if( ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_CLONES_ENTITY_ACROSS_PORTAL_FROM_MAIN) == 0) )
	{
		Assert( m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.Find( pEntity ) == -1 );
		return; //not cloned, no work to do
	}

	//NDebugOverlay::EntityBounds( pEntity, 255, 0, 0, 50, 5.0f );

	m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal.FindAndFastRemove( pEntity );
	m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] &= ~PSEF_CLONES_ENTITY_ACROSS_PORTAL_FROM_MAIN;

	//clear exported clones	
	if( m_pLinkedPortal )
	{
		for( int i = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			if( m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i]->GetClonedEntity() == pEntity )
			{
				CPhysicsShadowClone *pClone = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
				
				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
				m_pLinkedPortal->MarkAsReleased( pClone );
				pClone->Free();
				m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.FastRemove(i);

				break;
			}
		}
	}
}


/*void CPortalSimulator::TeleportEntityToLinkedPortal( CBaseEntity *pEntity )
{
	//TODO: migrate teleportation code from CPortal_Base2D::Touch to here


}*/






void CPortalSimulator::CreateAllPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CreateMinimumPhysics();
	CreateLocalPhysics();
	CreateLinkedPhysics();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::CreateMinimumPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment != NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateMinimumPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	m_InternalData.Simulation.pPhysicsEnvironment = physenv_main;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateMinimumPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::CreateLocalPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	AssertMsg( m_bLocalDataIsReady, "Portal simulator attempting to create local physics before being placed." );

	if( m_CreationChecklist.bLocalPhysicsGenerated )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CreateMinimumPhysics();

	//int iDefaultSurfaceIndex = physprops->GetSurfaceIndex( "default" );
	objectparams_t params = g_PhysDefaultObjectParams;

	// Any non-moving object can point to world safely-- Make sure we dont use 'params' for something other than that beyond this point.
	if( m_InternalData.Simulation.hCollisionEntity )
	{
		params.pGameData = m_InternalData.Simulation.hCollisionEntity;
	}
	else
	{
		params.pGameData = GetWorldEntity();
	}

	CPSCollisionEntity *pSetPhysicsObject = NULL;
	if( m_InternalData.Simulation.hCollisionEntity && (m_InternalData.Simulation.hCollisionEntity->VPhysicsGetObject() == NULL) )
	{
		pSetPhysicsObject = m_InternalData.Simulation.hCollisionEntity;
	}

	//World
	{
		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
		{
			Assert( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
			if( (m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable != NULL) &&
				((m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].iSolidMask & MASK_SOLID_BRUSHONLY) != 0) )
			{
				m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );

				if( pSetPhysicsObject )
				{
					pSetPhysicsObject->VPhysicsSetObject(m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject);
					pSetPhysicsObject = NULL;
				}

				m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
			}
		}

		if( m_InternalData.Simulation.Static.World.Displacements.pCollideable != NULL )
		{
			m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.World.Displacements.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );
			
			if( pSetPhysicsObject )
			{
				pSetPhysicsObject->VPhysicsSetObject(m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject);
				pSetPhysicsObject = NULL;
			}
			
			m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}

		//Assert( m_InternalData.Simulation.Static.World.StaticProps.PhysicsObjects.Count() == 0 ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
#ifdef _DEBUG
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		}
#endif
		
		if( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0 )
		{
			Assert( m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists );
			for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
			{
				PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
				Assert( Representation.pCollide != NULL );
				Assert( Representation.pPhysicsObject == NULL );
				
				Representation.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( Representation.pCollide, Representation.iTraceSurfaceProps, vec3_origin, vec3_angle, &params );
				Assert( Representation.pPhysicsObject != NULL );
				if( Representation.pPhysicsObject != NULL )
				{
					Representation.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
				}
				else
				{
					physcollision->DestroyCollide( Representation.pCollide );
					m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Remove( i );
				}
			}
		}
		m_InternalData.Simulation.Static.World.StaticProps.bPhysicsExists = true;
	}

	//Wall
	{
		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
		{
			Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
			if( (m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable != NULL) &&
				((m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].iSolidMask & MASK_SOLID_BRUSHONLY) != 0) )
			{
				m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );
				
				if( pSetPhysicsObject )
				{
					pSetPhysicsObject->VPhysicsSetObject(m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject);
					pSetPhysicsObject = NULL;
				}

				m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
			}
		}

		Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject == NULL );
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable != NULL )
		{
			m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );

			if( pSetPhysicsObject )
			{
				pSetPhysicsObject->VPhysicsSetObject(m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject);
				pSetPhysicsObject = NULL;
			}

			m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}

		Assert( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable != NULL )
		{
			m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, vec3_origin, vec3_angle, &params );
			
			if( pSetPhysicsObject )
			{
				pSetPhysicsObject->VPhysicsSetObject(m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject);
				pSetPhysicsObject = NULL;
			}

			m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}

		if( m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count() != 0 )
		{
			objectparams_t params = g_PhysDefaultObjectParams;

			Assert( m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists );
			for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
			{
				PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &Representation = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i];
				if( Representation.pCollide != NULL )
				{
					Assert( Representation.pPhysicsObject == NULL );

					ICollideable *pProp = Representation.pSourceEntity->GetCollideable();
					params.pGameData = m_InternalData.Simulation.hCollisionEntity;

					//add to the collision entity
					//Representation.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObject( Representation.pCollide, physprops->GetSurfaceIndex( "default" ), pProp->GetCollisionOrigin(), pProp->GetCollisionAngles(), &params );
					Representation.pPhysicsObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( Representation.pCollide, m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, pProp->GetCollisionOrigin(), pProp->GetCollisionAngles(), &params );
					Assert( Representation.pPhysicsObject != NULL );
					Representation.pPhysicsObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
				}
			}
		}
		m_InternalData.Simulation.Dynamic.CarvedEntities.bPhysicsExists = true;
	}

	//re-acquire environment physics for owned entities
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
		TakePhysicsOwnership( m_InternalData.Simulation.Dynamic.OwnedEntities[i] );

	if( m_InternalData.Simulation.hCollisionEntity )
	{
		m_InternalData.Simulation.hCollisionEntity->CollisionRulesChanged();
	}
	
	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalPhysicsGenerated = true;
}



void CPortalSimulator::CreateLinkedPhysics( void )
{
	if( IsSimulatingVPhysics() == false )
		return;

	AssertMsg( m_bLocalDataIsReady, "Portal simulator attempting to create linked physics before being placed itself." );

	if( (m_pLinkedPortal == NULL) || (m_pLinkedPortal->m_bLocalDataIsReady == false) )
		return;

	if( m_CreationChecklist.bLinkedPhysicsGenerated )
		return;

	CREATEDEBUGTIMER( functionTimer );
	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLinkedPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CreateMinimumPhysics();

	//int iDefaultSurfaceIndex = physprops->GetSurfaceIndex( "default" );
	objectparams_t params = g_PhysDefaultObjectParams;

	if( m_InternalData.Simulation.hCollisionEntity )
		params.pGameData = m_InternalData.Simulation.hCollisionEntity;
	else
		params.pGameData = GetWorldEntity();

	//everything in our linked collision should be based on the linked portal's world collision
	PS_SD_Static_World_t &RemoteSimulationStaticWorld = m_pLinkedPortal->m_InternalData.Simulation.Static.World;

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects ); ++iBrushSet )
	{
		Assert( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( (RemoteSimulationStaticWorld.Brushes.BrushSets[iBrushSet].pCollideable != NULL) &&
			((RemoteSimulationStaticWorld.Brushes.BrushSets[iBrushSet].iSolidMask & MASK_SOLID_BRUSHONLY) != 0) )
		{
			m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( RemoteSimulationStaticWorld.Brushes.BrushSets[iBrushSet].pCollideable, m_pLinkedPortal->m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps, m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform, m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform, &params );
			m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet]->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
		}
	}
	

	Assert( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count() == 0 ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( RemoteSimulationStaticWorld.StaticProps.ClippedRepresentations.Count() != 0 )
	{
		for( int i = RemoteSimulationStaticWorld.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = RemoteSimulationStaticWorld.StaticProps.ClippedRepresentations[i];
			IPhysicsObject *pPhysObject = m_InternalData.Simulation.pPhysicsEnvironment->CreatePolyObjectStatic( Representation.pCollide, Representation.iTraceSurfaceProps, m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform, m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform, &params );
			if( pPhysObject )
			{
				m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.AddToTail( pPhysObject );
				pPhysObject->RecheckCollisionFilter(); //some filters only work after the variable is stored in the class
			}
		}
	}

	//re-clone physicsshadowclones from the remote environment
	CUtlVector<CBaseEntity *> &RemoteOwnedEntities = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.OwnedEntities;
	for( int i = RemoteOwnedEntities.Count(); --i >= 0; )
	{
		if( CPhysicsShadowClone::IsShadowClone( RemoteOwnedEntities[i] ) ||
			CPSCollisionEntity::IsPortalSimulatorCollisionEntity( RemoteOwnedEntities[i] ) )
			continue;

		int j;
		for( j = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --j >= 0; )
		{
			if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[j]->GetClonedEntity() == RemoteOwnedEntities[i] )
				break;
		}

		if( j >= 0 ) //already cloning
			continue;
					
		

		EHANDLE hEnt = RemoteOwnedEntities[i];
		CPhysicsShadowClone *pClone = CPhysicsShadowClone::CreateShadowClone( m_InternalData.Simulation.pPhysicsEnvironment, hEnt, "CPortalSimulator::CreateLinkedPhysics(): From Linked Portal", &m_InternalData.Placement.matLinkedToThis.As3x4() );
		if( pClone )
		{
			MarkAsOwned( pClone );
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= PSEF_OWNS_PHYSICS | (m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[hEnt->entindex()] & PSEF_IS_IN_PORTAL_HOLE);
			m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.AddToTail( pClone );
			pClone->CollisionRulesChanged(); //adding the clone to the portal simulator changes how it collides
		}
	}

	CUtlVector<CBaseEntity *> &RemoteClonedEntities = m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.ShouldCloneToRemotePortal;
	for( int i = RemoteClonedEntities.Count(); --i >= 0; )
	{
		int j;
		for( j = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --j >= 0; )
		{
			if( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[j]->GetClonedEntity() == RemoteClonedEntities[i] )
				break;
		}

		if( j >= 0 ) //already cloning
			continue;

		EHANDLE hEnt = RemoteClonedEntities[i];
		CPhysicsShadowClone *pClone = CPhysicsShadowClone::CreateShadowClone( m_InternalData.Simulation.pPhysicsEnvironment, hEnt, "CPortalSimulator::CreateLinkedPhysics(): From Linked Portal", &m_InternalData.Placement.matLinkedToThis.As3x4() );
		if( pClone )
		{
			MarkAsOwned( pClone );
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] |= PSEF_OWNS_PHYSICS | (m_pLinkedPortal->m_InternalData.Simulation.Dynamic.EntFlags[hEnt->entindex()] & PSEF_IS_IN_PORTAL_HOLE);
			m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.AddToTail( pClone );
			pClone->CollisionRulesChanged(); //adding the clone to the portal simulator changes how it collides
		}
	}

	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->CreateLinkedPhysics();
		m_bInCrossLinkedFunction = false;
	}

	if( m_InternalData.Simulation.hCollisionEntity )
		m_InternalData.Simulation.hCollisionEntity->CollisionRulesChanged();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLinkedPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLinkedPhysicsGenerated = true;
}



void CPortalSimulator::ClearAllPhysics( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	ClearLinkedPhysics();
	ClearLocalPhysics();
	ClearMinimumPhysics();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::ClearMinimumPhysics( void )
{
	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearMinimumPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	m_InternalData.Simulation.pPhysicsEnvironment = NULL;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearMinimumPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::ClearLocalPhysics( void )
{
	if( m_CreationChecklist.bLocalPhysicsGenerated == false )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( true ); //if we don't do this, things crash the next time we cleanup the delete list while checking mindists
	
	if( m_InternalData.Simulation.hCollisionEntity )
	{
		m_InternalData.Simulation.hCollisionEntity->VPhysicsSetObject( NULL );
	}

	//world brushes
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject )
		{
			m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject );
			m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject = NULL;
		}
	}

	//world displacement surfaces
	if( m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject )
	{
		m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject );
		m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject = NULL;
	}

	//world static props
	if( m_InternalData.Simulation.Static.World.StaticProps.bPhysicsExists && 
		(m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0) )
	{
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
			if( Representation.pPhysicsObject )
			{
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( Representation.pPhysicsObject );
				Representation.pPhysicsObject = NULL;
			}
		}
	}
	m_InternalData.Simulation.Static.World.StaticProps.bPhysicsExists = false;


	//carved entities
	if( m_InternalData.Simulation.Dynamic.CarvedEntities.bPhysicsExists && 
		(m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count() != 0) )
	{
		for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &Representation = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i];
			if( Representation.pPhysicsObject )
			{
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( Representation.pPhysicsObject );
				Representation.pPhysicsObject = NULL;
			}
		}
	}
	m_InternalData.Simulation.Dynamic.CarvedEntities.bPhysicsExists = false;


	//wall brushes
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject )
		{
			m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject );
			m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject = NULL;
		}
	}

	//clipped func_clip_vphysics
	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject )
	{
		m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject );
		m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject = NULL;
	}

	//wall tube props
	if( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject )
	{
		m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject );
		m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject = NULL;
	}

	//all physics clones
	{
		for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			CPhysicsShadowClone *pClone = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
			Assert( GetSimulatorThatOwnsEntity( pClone ) == this );
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
			MarkAsReleased( pClone );
			Assert( GetSimulatorThatOwnsEntity( pClone ) == NULL );
			pClone->Free();
		}

		m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.RemoveAll();		
	}

	Assert( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0 );

	//release physics ownership of owned entities
	for( int i = m_InternalData.Simulation.Dynamic.OwnedEntities.Count(); --i >= 0; )
		ReleasePhysicsOwnership( m_InternalData.Simulation.Dynamic.OwnedEntities[i], false );

	Assert( m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0 );

	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( false );

	if( m_InternalData.Simulation.hCollisionEntity )
		m_InternalData.Simulation.hCollisionEntity->CollisionRulesChanged();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalPhysicsGenerated = false;
}



void CPortalSimulator::ClearLinkedPhysics( void )
{
	if( m_CreationChecklist.bLinkedPhysicsGenerated == false )
		return;

	if( m_InternalData.Simulation.pPhysicsEnvironment == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLinkedPhysics() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( true ); //if we don't do this, things crash the next time we cleanup the delete list while checking mindists

	//static collideables
	{
		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects ); ++iBrushSet )
		{
			if( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] )
			{
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] );
				m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] = NULL;
			}
		}

		if( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count() )
		{
			for( int i = m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count(); --i >= 0; )
				m_InternalData.Simulation.pPhysicsEnvironment->DestroyObject( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects[i] );
			
			m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.RemoveAll();
		}
	}

	//clones from the linked portal
	{
		for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			CPhysicsShadowClone *pClone = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
			MarkAsReleased( pClone );
			pClone->Free();
		}

		m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.RemoveAll();
	}


	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->ClearLinkedPhysics();
		m_bInCrossLinkedFunction = false;
	}

	Assert( (m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0) && 
		((m_pLinkedPortal == NULL) || (m_pLinkedPortal->m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count() == 0)) );

	m_InternalData.Simulation.pPhysicsEnvironment->CleanupDeleteList();
	m_InternalData.Simulation.pPhysicsEnvironment->SetQuickDelete( false );

	if( m_InternalData.Simulation.hCollisionEntity )
		m_InternalData.Simulation.hCollisionEntity->CollisionRulesChanged();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLinkedPhysics() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLinkedPhysicsGenerated = false;
}


void CPortalSimulator::ClearLinkedEntities( void )
{
	//clones from the linked portal
	{
		for( int i = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.Count(); --i >= 0; )
		{
			CPhysicsShadowClone *pClone = m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal[i];
			m_InternalData.Simulation.Dynamic.EntFlags[pClone->entindex()] &= ~PSEF_OWNS_PHYSICS;
			MarkAsReleased( pClone );
			pClone->Free();
		}

		m_InternalData.Simulation.Dynamic.ShadowClones.FromLinkedPortal.RemoveAll();
	}
}
#endif //#ifndef CLIENT_DLL

void CPortalSimulator::SetCollisionGenerationEnabled( bool bEnabled )
{
	if( bEnabled != m_bGenerateCollision )
	{
		m_bGenerateCollision = bEnabled;
		if( bEnabled )
		{
			CreatePolyhedrons();
			CreateAllCollision();
#ifndef CLIENT_DLL
			CreateAllPhysics();
#endif
		}
		else
		{
#ifndef CLIENT_DLL
			ClearAllPhysics();
#endif
			ClearAllCollision();
			ClearPolyhedrons();
		}
	}
}


void CPortalSimulator::CreateAllCollision( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	CreateLocalCollision();
	CreateLinkedCollision();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateAllCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::CreateLocalCollision( void )
{
	AssertMsg( m_bLocalDataIsReady, "Portal simulator attempting to create local collision before being placed." );

	if( m_CreationChecklist.bLocalCollisionGenerated )
		return;

	if( IsCollisionGenerationEnabled() == false )
		return;

	DEBUGTIMERONLY( s_iPortalSimulatorGUID = GetPortalSimulatorGUID() );

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	CREATEDEBUGTIMER( worldBrushTimer );
	STARTDEBUGTIMER( worldBrushTimer );
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		Assert( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() != 0 )
		{
			m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.Base(), m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() );
		}
	}
	STOPDEBUGTIMER( worldBrushTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWorld Brushes=%fms\n", GetPortalSimulatorGUID(), TABSPACING, worldBrushTimer.GetDuration().GetMillisecondsF() ); );

	// Displacements
	if ( portal_clone_displacements.GetBool() )
	{
		VPlane displacementRejectRegions[6];
		displacementRejectRegions[0].m_Normal = -m_InternalData.Placement.vForward;
		displacementRejectRegions[0].m_Dist = displacementRejectRegions[0].m_Normal.Dot( m_InternalData.Placement.ptCenter );
		displacementRejectRegions[1].m_Normal = m_InternalData.Placement.vForward;
		displacementRejectRegions[1].m_Dist = displacementRejectRegions[1].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.x;
		displacementRejectRegions[2].m_Normal = m_InternalData.Placement.vRight;
		displacementRejectRegions[2].m_Dist = displacementRejectRegions[2].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.y;
		displacementRejectRegions[3].m_Normal = -m_InternalData.Placement.vRight;
		displacementRejectRegions[3].m_Dist = displacementRejectRegions[3].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.y;
		displacementRejectRegions[4].m_Normal = m_InternalData.Placement.vUp;
		displacementRejectRegions[4].m_Dist = displacementRejectRegions[4].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.z;
		displacementRejectRegions[5].m_Normal = -m_InternalData.Placement.vUp;
		displacementRejectRegions[5].m_Dist = displacementRejectRegions[5].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.z;

		CREATEDEBUGTIMER( dispTimer );
		STARTDEBUGTIMER( dispTimer );
		Assert( m_InternalData.Simulation.Static.World.Displacements.pCollideable == NULL );
		virtualmeshlist_t DisplacementMeshes[32];

		int iMeshes = enginetrace->GetMeshesFromDisplacementsInAABB( m_InternalData.Placement.vecCurAABBMins, m_InternalData.Placement.vecCurAABBMaxs, DisplacementMeshes, ARRAYSIZE(DisplacementMeshes) );
		if( iMeshes > 0 )
		{
			CPhysPolysoup *pDispCollideSoup = physcollision->PolysoupCreate();

			// Count total triangles added to this poly soup- Can't support more than 65535.
			int iTriCount = 0;

			for( int i = 0; (i != iMeshes) && (iTriCount < 65535); ++i )
			{
				virtualmeshlist_t *pMesh = &DisplacementMeshes[i];

				for ( int j = 0; j < pMesh->indexCount; j+=3 )
				{
					Vector *points[3] = { &pMesh->pVerts[ pMesh->indices[j+0] ],  &pMesh->pVerts[ pMesh->indices[j+1] ],  &pMesh->pVerts[ pMesh->indices[j+2] ] };					

					//test for triangles that lie completely outside our collision area
					{
						int k;
						for( k = 0; k != ARRAYSIZE( displacementRejectRegions ); ++k )
						{
							//test all 3 points on each plane
							if( (displacementRejectRegions[k].DistTo( *points[0] ) >= 0.0f) &&
								(displacementRejectRegions[k].DistTo( *points[1] ) >= 0.0f) &&
								(displacementRejectRegions[k].DistTo( *points[2] ) >= 0.0f) )
							{
								break; //break out if all 3 are in front of a rejection plane
							}
						}

						if( k != ARRAYSIZE( displacementRejectRegions ) )
						{
							//was fully rejected by a plane
							continue;
						}
					}

					//clip to portal plane
					{
						//we do however need to clip to the wall plane
						int iFront = 0;
						int iBack = 0;
						float fDists[3];
						int iForwardPoints[3];
						int iBackPoints[3];
						for( int k = 0; k != 3; ++k )
						{
							fDists[k] = m_InternalData.Placement.PortalPlane.DistTo( *points[k] );
							if( fDists[k] >= 0.0f )
							{
								iForwardPoints[iFront] = k;
								++iFront;
							}
							else
							{
								iBackPoints[iBack] = k;
								++iBack;
							}
						}
						if( iFront != 0 )
						{
							if( iBack != 0 )
							{
								//need to clip the triangle
								Vector vClippedPoints[2]; //guaranteed to intersect exactly twice
								
								if( iBack == 2 )
								{
									if( fDists[iForwardPoints[0]] < 0.1f )
										continue;

									//easy case.
									float fTotalDist = fDists[iForwardPoints[0]] - fDists[iBackPoints[0]];
									if( fTotalDist < 0.1f )
										continue;

									vClippedPoints[0] = ((*points[iBackPoints[0]]) * (fDists[iForwardPoints[0]]/fTotalDist)) - ((*points[iForwardPoints[0]]) * (fDists[iBackPoints[0]]/fTotalDist));
									points[iBackPoints[0]] = &vClippedPoints[0];

									fTotalDist = fDists[iForwardPoints[0]] - fDists[iBackPoints[1]];
									if( fTotalDist < 0.1f )
										continue;

									vClippedPoints[1] = ((*points[iBackPoints[1]]) * (fDists[iForwardPoints[0]]/fTotalDist)) - ((*points[iForwardPoints[0]]) * (fDists[iBackPoints[1]]/fTotalDist));
									points[iBackPoints[1]] = &vClippedPoints[1];

									physcollision->PolysoupAddTriangle( pDispCollideSoup, *points[0], *points[1], *points[2], pMesh->surfacePropsIndex );
									++iTriCount;
								}
								else
								{
									if( fDists[iBackPoints[0]] > -0.1f )
									{
										physcollision->PolysoupAddTriangle( pDispCollideSoup, *points[0], *points[1], *points[2], pMesh->surfacePropsIndex );
										++iTriCount;
										continue;
									}

									//need to create 2 triangles
									float fTotalDist = fDists[iForwardPoints[0]] - fDists[iBackPoints[0]];									
									vClippedPoints[0] = ((*points[iBackPoints[0]]) * (fDists[iForwardPoints[0]]/fTotalDist)) - ((*points[iForwardPoints[0]]) * (fDists[iBackPoints[0]]/fTotalDist));
									fTotalDist = fDists[iForwardPoints[1]] - fDists[iBackPoints[0]];									
									vClippedPoints[1] = ((*points[iBackPoints[0]]) * (fDists[iForwardPoints[1]]/fTotalDist)) - ((*points[iForwardPoints[1]]) * (fDists[iBackPoints[0]]/fTotalDist));

									points[iBackPoints[0]] = &vClippedPoints[0];
									physcollision->PolysoupAddTriangle( pDispCollideSoup, *points[0], *points[1], *points[2], pMesh->surfacePropsIndex );
									++iTriCount;

									points[iBackPoints[0]] = &vClippedPoints[1];
									points[iForwardPoints[0]] = &vClippedPoints[0];
									physcollision->PolysoupAddTriangle( pDispCollideSoup, *points[0], *points[1], *points[2], pMesh->surfacePropsIndex );
									++iTriCount;
								}
							}
							else
							{
								//triangle resides wholly in front of the portal plane
								physcollision->PolysoupAddTriangle( pDispCollideSoup, *points[0], *points[1], *points[2], pMesh->surfacePropsIndex );
								++iTriCount;
							}

							if( iTriCount >= 65535 )
							{
								break;
							}
						}
					}

				}// triangle loop
			}

			m_InternalData.Simulation.Static.World.Displacements.pCollideable = physcollision->ConvertPolysoupToCollide( pDispCollideSoup, false );

			// clean up poly soup
			physcollision->PolysoupDestroy( pDispCollideSoup );
		}

		//m_InternalData.Simulation.Static.World.Displacements.pCollideable = enginetrace->GetCollidableFromDisplacementsInAABB( m_InternalData.Placement.vecCurAABBMins, m_InternalData.Placement.vecCurAABBMaxs );
		STOPDEBUGTIMER( dispTimer );
		DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sDisplacement Surfaces=%fms\n", GetPortalSimulatorGUID(), TABSPACING, dispTimer.GetDuration().GetMillisecondsF() ); );
	}

	//static props
	CREATEDEBUGTIMER( worldPropTimer );
	STARTDEBUGTIMER( worldPropTimer );
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
	{
		Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide == NULL );
	}
#endif
	Assert( m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists == false ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0 )
	{
		Assert( m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() != 0 );
		CPolyhedron **pPolyhedronsBase = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Base();
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
			
			Assert( Representation.pCollide == NULL );
			Representation.pCollide = ConvertPolyhedronsToCollideable( &pPolyhedronsBase[Representation.PolyhedronGroup.iStartIndex], Representation.PolyhedronGroup.iNumPolyhedrons );

			Assert( Representation.pCollide != NULL );
			if( Representation.pCollide == NULL )
			{
				//we really shouldn't get here, but we do sometimes. Ideally we should either solve the conversion from polyhedrons to collideables, or throw away the polyhedrons as we carve them.
				m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Remove( i ); //this will temporarily leak the polyhedrons we're referencing. But they'll get removed en-masse with the rest of the static prop polyhedrons when we move or destruct
			}
		}
	}
	m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists = true;
	STOPDEBUGTIMER( worldPropTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWorld Props=%fms\n", GetPortalSimulatorGUID(), TABSPACING, worldPropTimer.GetDuration().GetMillisecondsF() ); );

	//carved entities
	CREATEDEBUGTIMER( worldEntityTimer );
	STARTDEBUGTIMER( worldEntityTimer );
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
	{
		Assert( m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pCollide == NULL );
	}
#endif
	Assert( m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists == false ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count() != 0 )
	{
		Assert( m_InternalData.Simulation.Dynamic.CarvedEntities.Polyhedrons.Count() != 0 );
		CPolyhedron **pPolyhedronsBase = m_InternalData.Simulation.Dynamic.CarvedEntities.Polyhedrons.Base();
		for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &Representation = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i];

			if( Representation.CarvedPolyhedronGroup.iNumPolyhedrons != 0 )
			{
				Assert( Representation.pCollide == NULL );
				Representation.pCollide = ConvertPolyhedronsToCollideable( &pPolyhedronsBase[Representation.CarvedPolyhedronGroup.iStartIndex], Representation.CarvedPolyhedronGroup.iNumPolyhedrons );
				Assert( Representation.pCollide != NULL );
			}
		}
	}
	m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists = true;
	STOPDEBUGTIMER( worldEntityTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWorld Entities=%fms\n", GetPortalSimulatorGUID(), TABSPACING, worldEntityTimer.GetDuration().GetMillisecondsF() ); );


	//TODO: replace the complete wall with the wall shell
	CREATEDEBUGTIMER( wallBrushTimer );
	STARTDEBUGTIMER( wallBrushTimer );
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() != 0 )
		{
			m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.Base(), m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() );
		}
	}
	STOPDEBUGTIMER( wallBrushTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWall Brushes=%fms\n", GetPortalSimulatorGUID(), TABSPACING, wallBrushTimer.GetDuration().GetMillisecondsF() ); );


#if defined( GAME_DLL )
	CREATEDEBUGTIMER( func_clip_vphysics_timer );
	STARTDEBUGTIMER( func_clip_vphysics_timer );	
	Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons.Count() != 0 )
	{
		m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons.Base(), m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons.Count() );
	}
	STOPDEBUGTIMER( func_clip_vphysics_timer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sfunc_clip_vphysics Brushes=%fms\n", GetPortalSimulatorGUID(), TABSPACING, func_clip_vphysics_timer.GetDuration().GetMillisecondsF() ); );
#endif


	CREATEDEBUGTIMER( wallTubeTimer );
	STARTDEBUGTIMER( wallTubeTimer );
	Assert( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable == NULL ); //Be sure to find graceful fixes for asserts, performance is a big concern with portal simulation
	if( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() != 0 )
		m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable = ConvertPolyhedronsToCollideable( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Base(), m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() );
	STOPDEBUGTIMER( wallTubeTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sWall Tube=%fms\n", GetPortalSimulatorGUID(), TABSPACING, wallTubeTimer.GetDuration().GetMillisecondsF() ); );

	//grab surface properties to use for the portal environment
	{
		CTraceFilterWorldAndPropsOnly filter;
		trace_t Trace;
		UTIL_TraceLine( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vForward, m_InternalData.Placement.ptCenter - (m_InternalData.Placement.vForward * 500.0f), MASK_SOLID_BRUSHONLY, &filter, &Trace );

		if( Trace.fraction != 1.0f )
		{
			m_InternalData.Simulation.Static.SurfaceProperties.contents = Trace.contents;
			m_InternalData.Simulation.Static.SurfaceProperties.surface = Trace.surface;
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = Trace.m_pEnt;
		}
		else
		{
			m_InternalData.Simulation.Static.SurfaceProperties.contents = CONTENTS_SOLID;
			m_InternalData.Simulation.Static.SurfaceProperties.surface.name = "**empty**";
			m_InternalData.Simulation.Static.SurfaceProperties.surface.flags = 0;
			m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps = 0;
#ifndef CLIENT_DLL
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = GetWorldEntity();
#else
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = GetClientWorldEntity();
#endif
		}

		if( m_InternalData.Simulation.hCollisionEntity )
			m_InternalData.Simulation.Static.SurfaceProperties.pEntity = m_InternalData.Simulation.hCollisionEntity;
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreateLocalCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalCollisionGenerated = true;
}



void CPortalSimulator::CreateLinkedCollision( void )
{
	if( m_CreationChecklist.bLinkedCollisionGenerated )
		return;

	if( IsCollisionGenerationEnabled() == false )
		return;

	//nothing to do for now, the current set of collision is just transformed from the linked simulator when needed. It's really cheap to transform in traces and physics generation.
	
	m_CreationChecklist.bLinkedCollisionGenerated = true;
}



void CPortalSimulator::ClearAllCollision( void )
{
	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	ClearLinkedCollision();
	ClearLocalCollision();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearAllCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}



void CPortalSimulator::ClearLinkedCollision( void )
{
	if( m_CreationChecklist.bLinkedCollisionGenerated == false )
		return;

	//nothing to do for now, the current set of collision is just transformed from the linked simulator when needed. It's really cheap to transform in traces and physics generation.
	
	m_CreationChecklist.bLinkedCollisionGenerated = false;
}

void CPortalSimulator::ClearLocalCollision( void )
{
	if( m_CreationChecklist.bLocalCollisionGenerated == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalCollision() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		DestroyCollideable( &m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable );
	}

#if defined( GAME_DLL )
	DestroyCollideable( &m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable );
#endif

	DestroyCollideable( &m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable );

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		DestroyCollideable( &m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable );
	}

	DestroyCollideable( &m_InternalData.Simulation.Static.World.Displacements.pCollideable );


	if( m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists && 
		(m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count() != 0) )
	{
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Static_World_StaticProps_ClippedProp_t &Representation = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i];
			DestroyCollideable( &Representation.pCollide );
		}
	}
	m_InternalData.Simulation.Static.World.StaticProps.bCollisionExists = false;

	//carved entities
	if( m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists && 
		(m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count() != 0) )
	{
		for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
		{
			PS_SD_Dynamic_CarvedEntities_CarvedEntity_t &Representation = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i];
			DestroyCollideable( &Representation.pCollide );
		}
	}
	m_InternalData.Simulation.Dynamic.CarvedEntities.bCollisionExists = false;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearLocalCollision() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bLocalCollisionGenerated = false;
}



void CPortalSimulator::CreatePolyhedrons( void )
{
	if( m_CreationChecklist.bPolyhedronsGenerated )
		return;

	if( IsCollisionGenerationEnabled() == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreatePolyhedrons() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();

	const float fHalfHoleWidth = m_InternalData.Placement.fHalfWidth + PORTAL_HOLE_HALF_WIDTH_MOD;
	const float fHalfHoleHeight = m_InternalData.Placement.fHalfHeight + PORTAL_HOLE_HALF_HEIGHT_MOD;

	//forward reverse conventions signify whether the normal is the same direction as m_InternalData.Placement.PortalPlane.m_Normal
	//World and wall conventions signify whether it's been shifted in front of the portal plane or behind it

	float fWorldClipPlane_Forward[4] = {	m_InternalData.Placement.PortalPlane.m_Normal.x,
											m_InternalData.Placement.PortalPlane.m_Normal.y,
											m_InternalData.Placement.PortalPlane.m_Normal.z,
											m_InternalData.Placement.PortalPlane.m_Dist + PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT };

	float fWorldClipPlane_Reverse[4] = {	-fWorldClipPlane_Forward[0],
											-fWorldClipPlane_Forward[1],
											-fWorldClipPlane_Forward[2],
											-fWorldClipPlane_Forward[3] };

	float fWallClipPlane_Forward[4] = {		m_InternalData.Placement.PortalPlane.m_Normal.x,
											m_InternalData.Placement.PortalPlane.m_Normal.y,
											m_InternalData.Placement.PortalPlane.m_Normal.z,
											m_InternalData.Placement.PortalPlane.m_Dist }; // - PORTAL_WORLD_WALL_HALF_SEPARATION_AMOUNT

	//float fWallClipPlane_Reverse[4] = {		-fWallClipPlane_Forward[0],
	//										-fWallClipPlane_Forward[1],
	//										-fWallClipPlane_Forward[2],
	//										-fWallClipPlane_Forward[3] };

	VPlane collisionClip[6];
	collisionClip[0].m_Normal = *(Vector *)fWorldClipPlane_Reverse;
	collisionClip[0].m_Dist = fWorldClipPlane_Reverse[3];
	collisionClip[1].m_Normal = m_InternalData.Placement.vForward;
	collisionClip[1].m_Dist = collisionClip[1].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.x;
	collisionClip[2].m_Normal = m_InternalData.Placement.vRight;
	collisionClip[2].m_Dist = collisionClip[2].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.y;
	collisionClip[3].m_Normal = -m_InternalData.Placement.vRight;
	collisionClip[3].m_Dist = collisionClip[3].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.y;
	collisionClip[4].m_Normal = m_InternalData.Placement.vUp;
	collisionClip[4].m_Dist = collisionClip[4].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.z;
	collisionClip[5].m_Normal = -m_InternalData.Placement.vUp;
	collisionClip[5].m_Dist = collisionClip[5].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.z;


	//World
	{
		Vector vOBBForward = m_InternalData.Placement.vForward;
		Vector vOBBRight = m_InternalData.Placement.vRight;
		Vector vOBBUp = m_InternalData.Placement.vUp;

		vOBBForward *= m_InternalData.Placement.vCollisionCloneExtents.x;
		vOBBRight *= m_InternalData.Placement.vCollisionCloneExtents.y;
		vOBBUp *= m_InternalData.Placement.vCollisionCloneExtents.z;

		Vector ptOBBOrigin = m_InternalData.Placement.ptCenter;
		ptOBBOrigin -= vOBBRight;
		ptOBBOrigin -= vOBBUp;

		vOBBRight *= 2.0f;
		vOBBUp *= 2.0f;

		Vector vAABBMins, vAABBMaxs;
		vAABBMins = vAABBMaxs = ptOBBOrigin;

		for( int i = 1; i != 8; ++i )
		{
			Vector ptTest = ptOBBOrigin;
			if( i & (1 << 0) ) ptTest += vOBBForward;
			if( i & (1 << 1) ) ptTest += vOBBRight;
			if( i & (1 << 2) ) ptTest += vOBBUp;

			if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
			if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
			if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
			if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
			if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
			if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
		}

		m_InternalData.Placement.vecCurAABBMins = vAABBMins;
		m_InternalData.Placement.vecCurAABBMaxs = vAABBMaxs;

		//Brushes
		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
		{
			Assert( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() == 0 );

			//CUtlVector<int> WorldBrushes;
			CBrushQuery WorldBrushes;
			enginetrace->GetBrushesInAABB( vAABBMins, vAABBMaxs, WorldBrushes, m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].iSolidMask );

			//create locally clipped polyhedrons for the world
			{
				uint32 *pBrushList = WorldBrushes.Base();
				int iBrushCount = WorldBrushes.Count();
				ConvertBrushListToClippedPolyhedronList( pBrushList, iBrushCount, (float *)collisionClip, ARRAYSIZE( collisionClip ), PORTAL_POLYHEDRON_CUT_EPSILON, &m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons );
			}
		}

		//static props
		{
			Assert( m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() == 0 );

			CUtlVector<ICollideable *> StaticProps;
			staticpropmgr->GetAllStaticPropsInAABB( vAABBMins, vAABBMaxs, &StaticProps );
			
			for( int i = StaticProps.Count(); --i >= 0; )
			{
				ICollideable *pProp = StaticProps[i];

				// Don't consider props that aren't solid!
				if ( pProp->GetSolid() == SOLID_NONE || (pProp->GetSolidFlags() & FSOLID_NOT_SOLID) )
					continue;

				VPlane transformedCollisionClip[6];

				//TODO: should be able to just strip out the VectorRotate() math
				const VMatrix matCollisionToWorld( pProp->CollisionToWorldTransform() );
				matrix3x4_t matWorldToCollision_RotationOnly;
				MatrixTranspose( matCollisionToWorld.As3x4(), matWorldToCollision_RotationOnly );
				Vector vPropTranslation = matCollisionToWorld.GetTranslation();

				for( int clip = 0; clip != ARRAYSIZE( collisionClip ); ++clip )
				{
					VectorRotate( collisionClip[clip].m_Normal, matWorldToCollision_RotationOnly, transformedCollisionClip[clip].m_Normal );
					transformedCollisionClip[clip].m_Dist = collisionClip[clip].m_Dist - collisionClip[clip].m_Normal.Dot( vPropTranslation );
				}

				const CPolyhedron *PolyhedronArray[1024];
				int iPolyhedronCount = g_StaticCollisionPolyhedronCache.GetStaticPropPolyhedrons( pProp, PolyhedronArray, 1024 );

				PropPolyhedronGroup_t indices;
				indices.iStartIndex = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count();

				for( int j = 0; j != iPolyhedronCount; ++j )
				{					
					CPolyhedron *pClippedPropPolyhedron = ClipPolyhedron( PolyhedronArray[j], (float *)transformedCollisionClip, 6, PORTAL_WORLDCLIP_EPSILON, false );
					if( pClippedPropPolyhedron )
					{
						//transform the output polyhedron into world space
						for( int k = 0; k != pClippedPropPolyhedron->iVertexCount; ++k )
						{
							pClippedPropPolyhedron->pVertices[k] = matCollisionToWorld * pClippedPropPolyhedron->pVertices[k];							
						}

						for( int k = 0; k != pClippedPropPolyhedron->iPolygonCount; ++k )
						{
							pClippedPropPolyhedron->pPolygons[k].polyNormal = matCollisionToWorld.ApplyRotation( pClippedPropPolyhedron->pPolygons[k].polyNormal );
						}

						m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.AddToTail( pClippedPropPolyhedron );
					}
				}

				g_StaticCollisionPolyhedronCache.ReleaseStaticPropPolyhedrons( pProp, PolyhedronArray, iPolyhedronCount );

				indices.iNumPolyhedrons = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() - indices.iStartIndex;
				if( indices.iNumPolyhedrons != 0 )
				{
					int index = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.AddToTail();
					PS_SD_Static_World_StaticProps_ClippedProp_t &NewEntry = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[index];

					NewEntry.PolyhedronGroup = indices;
					NewEntry.pCollide = NULL;
#ifndef CLIENT_DLL
					NewEntry.pPhysicsObject = NULL;
#endif
					NewEntry.pSourceProp = pProp->GetEntityHandle();

					const model_t *pModel = pProp->GetCollisionModel();
					bool bIsStudioModel = pModel && (modelinfo->GetModelType( pModel ) == mod_studio);
					AssertOnce( bIsStudioModel );
					if( bIsStudioModel )
					{
						studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pModel );
						Assert( pStudioHdr != NULL );
						NewEntry.iTraceContents = pStudioHdr->contents;						
						NewEntry.iTraceSurfaceProps = pStudioHdr->GetSurfaceProp();
					}
					else
					{
						NewEntry.iTraceContents = m_InternalData.Simulation.Static.SurfaceProperties.contents;
						NewEntry.iTraceSurfaceProps = m_InternalData.Simulation.Static.SurfaceProperties.surface.surfaceProps;
					}
				}
			}
		}
	}

	//carved entities
	{
		for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
		{
			Assert( (m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].CarvedPolyhedronGroup.iNumPolyhedrons == 0) && (m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].UncarvedPolyhedronGroup.iNumPolyhedrons == 0) );
			CarveEntity( m_InternalData.Placement, m_InternalData.Simulation.Dynamic.CarvedEntities, m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i] );
		}
	}

	//(Holy) Wall
	{
		Vector vBackward = -m_InternalData.Placement.vForward;
		Vector vLeft = -m_InternalData.Placement.vRight;
		Vector vDown = -m_InternalData.Placement.vUp;

		Vector vOBBForward = -m_InternalData.Placement.vForward;
		Vector vOBBRight = -m_InternalData.Placement.vRight;
		Vector vOBBUp = m_InternalData.Placement.vUp;

		//scale the extents to usable sizes
		vOBBForward *= MAX( m_InternalData.Placement.fHalfHeight, m_InternalData.Placement.fHalfWidth ) * 2.0f;
		vOBBRight *= m_InternalData.Placement.fHalfWidth * 8.0f;
		vOBBUp *= m_InternalData.Placement.fHalfHeight * 8.0f;

		Vector ptOBBOrigin = m_InternalData.Placement.ptCenter;
		ptOBBOrigin -= vOBBRight / 2.0f;
		ptOBBOrigin -= vOBBUp / 2.0f;

		Vector vAABBMins, vAABBMaxs;
		vAABBMins = vAABBMaxs = ptOBBOrigin;

		for( int i = 1; i != 8; ++i )
		{
			Vector ptTest = ptOBBOrigin;
			if( i & (1 << 0) ) ptTest += vOBBForward;
			if( i & (1 << 1) ) ptTest += vOBBRight;
			if( i & (1 << 2) ) ptTest += vOBBUp;

			if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
			if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
			if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
			if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
			if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
			if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
		}


		float fPlanes[6 * 4];

		//first and second planes are always forward and backward planes
		fPlanes[(0*4) + 0] = fWallClipPlane_Forward[0];
		fPlanes[(0*4) + 1] = fWallClipPlane_Forward[1];
		fPlanes[(0*4) + 2] = fWallClipPlane_Forward[2];
		fPlanes[(0*4) + 3] = fWallClipPlane_Forward[3];

		fPlanes[(1*4) + 0] = vBackward.x;
		fPlanes[(1*4) + 1] = vBackward.y;
		fPlanes[(1*4) + 2] = vBackward.z;
		fPlanes[(1*4) + 3] = vBackward.Dot( m_InternalData.Placement.ptCenter ) + 1.0f;


		//the remaining planes will always have the same ordering of normals, with different distances plugged in for each convex we're creating
		//normal order is up, down, left, right

		fPlanes[(2*4) + 0] = m_InternalData.Placement.vUp.x;
		fPlanes[(2*4) + 1] = m_InternalData.Placement.vUp.y;
		fPlanes[(2*4) + 2] = m_InternalData.Placement.vUp.z;
		fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;

		fPlanes[(3*4) + 0] = vDown.x;
		fPlanes[(3*4) + 1] = vDown.y;
		fPlanes[(3*4) + 2] = vDown.z;
		fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;

		fPlanes[(4*4) + 0] = vLeft.x;
		fPlanes[(4*4) + 1] = vLeft.y;
		fPlanes[(4*4) + 2] = vLeft.z;
		fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleWidth;

		fPlanes[(5*4) + 0] = m_InternalData.Placement.vRight.x;
		fPlanes[(5*4) + 1] = m_InternalData.Placement.vRight.y;
		fPlanes[(5*4) + 2] = m_InternalData.Placement.vRight.z;
		fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleWidth;

		

		//these 2 get re-used a bit
		float fFarRightPlaneDistance = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter + m_InternalData.Placement.vRight * (m_InternalData.Placement.fHalfWidth * 40.0f) );
		float fFarLeftPlaneDistance = vLeft.Dot( m_InternalData.Placement.ptCenter + vLeft * (m_InternalData.Placement.fHalfHeight * 40.0f) );

		collisionClip[0].m_Normal = -m_InternalData.Placement.vForward;
		collisionClip[0].m_Dist = collisionClip[0].m_Normal.Dot( m_InternalData.Placement.ptCenter ) + m_InternalData.Placement.vCollisionCloneExtents.x;
		collisionClip[1].m_Normal = *(Vector *)fWallClipPlane_Forward;
		collisionClip[1].m_Dist = fWallClipPlane_Forward[3];

		CUtlVector<CPolyhedron *> WallBrushPolyhedrons_ClippedToWall;

		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
		{
			Assert( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() == 0 );

			//CUtlVector<int> WallBrushes;
			CBrushQuery WallBrushes;
			
			enginetrace->GetBrushesInAABB( vAABBMins, vAABBMaxs, WallBrushes, m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].iSolidMask );

			if( WallBrushes.Count() != 0 )
				ConvertBrushListToClippedPolyhedronList( WallBrushes.Base(), WallBrushes.Count(), (float *)collisionClip, ARRAYSIZE( collisionClip ), PORTAL_POLYHEDRON_CUT_EPSILON, &WallBrushPolyhedrons_ClippedToWall );
			
			CarveWallBrushes_Sub( fPlanes, WallBrushPolyhedrons_ClippedToWall, m_InternalData, m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons, fFarRightPlaneDistance, fFarLeftPlaneDistance, vLeft, vDown );

			for( int i = WallBrushPolyhedrons_ClippedToWall.Count(); --i >= 0; )
				WallBrushPolyhedrons_ClippedToWall[i]->Release();

			WallBrushPolyhedrons_ClippedToWall.RemoveAll();
		}

#if defined( GAME_DLL )
		//func_clip_vphysics
		if( portal_carve_vphysics_clips.GetBool() )
		{
			s_VPhysicsClipWatcher.Cache();
			for( int i = 0; i != s_VPhysicsClipWatcher.m_VPhysicsClipEntities.Count(); ++i )
			{
				VPhysicsClipEntry_t &checkEntry = s_VPhysicsClipWatcher.m_VPhysicsClipEntities[i];

				if( !((checkEntry.vAABBMins.x >= vAABBMaxs.x) ||
					(checkEntry.vAABBMins.y >= vAABBMaxs.y) ||
					(checkEntry.vAABBMins.z >= vAABBMaxs.z) ||
					(checkEntry.vAABBMaxs.x <= vAABBMins.x) ||
					(checkEntry.vAABBMaxs.y <= vAABBMins.y) ||
					(checkEntry.vAABBMaxs.z <= vAABBMins.z)) )
				{
					CBaseEntity *pClip = checkEntry.hEnt;
					if( pClip && (pClip->GetMoveParent() == NULL) )
					{

						CCollisionProperty *pProp = pClip->CollisionProp();
						if( pProp )
						{
							SolidType_t solidType = pClip->GetSolid();
							if( solidType == SOLID_VPHYSICS )
							{
								vcollide_t *pCollide = modelinfo->GetVCollide( pProp->GetCollisionModelIndex() );
								Assert( pCollide != NULL );
								if( pCollide != NULL )
								{
									CPhysConvex *ConvexesArray[1024];
									int iConvexCount = 0;
									for( int i = 0; i != pCollide->solidCount; ++i )
									{
										iConvexCount += physcollision->GetConvexesUsedInCollideable( pCollide->solids[i], ConvexesArray, 1024 - iConvexCount );
									}

									for( int j = 0; j != iConvexCount; ++j )
									{
										CPolyhedron *pFullPolyhedron = physcollision->PolyhedronFromConvex( ConvexesArray[j], true );
										if( pFullPolyhedron != NULL )
										{
											CPolyhedron *pClippedPolyhedron = ClipPolyhedron( pFullPolyhedron, (float *)collisionClip, ARRAYSIZE( collisionClip ), PORTAL_POLYHEDRON_CUT_EPSILON, false );
											if( pClippedPolyhedron )
											{
												WallBrushPolyhedrons_ClippedToWall.AddToTail( pClippedPolyhedron );
											}
											pFullPolyhedron->Release();
										}
									}
								}
							}
							else if( solidType == SOLID_BSP )
							{
								CBrushQuery brushQuery;
								enginetrace->GetBrushesInCollideable( pProp, brushQuery );

								if( brushQuery.Count() != 0 )
									ConvertBrushListToClippedPolyhedronList( brushQuery.Base(), brushQuery.Count(), (float *)collisionClip, ARRAYSIZE( collisionClip ), PORTAL_POLYHEDRON_CUT_EPSILON, &WallBrushPolyhedrons_ClippedToWall );
							}
						}
					}
				}
			}

			CarveWallBrushes_Sub( fPlanes, WallBrushPolyhedrons_ClippedToWall, m_InternalData, m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons, fFarRightPlaneDistance, fFarLeftPlaneDistance, vLeft, vDown );

			for( int i = WallBrushPolyhedrons_ClippedToWall.Count(); --i >= 0; )
				WallBrushPolyhedrons_ClippedToWall[i]->Release();

			WallBrushPolyhedrons_ClippedToWall.RemoveAll();
		}
#endif
	}

	CreateTubePolyhedrons();

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::CreatePolyhedrons() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bPolyhedronsGenerated = true;
}

void CarveWallBrushes_Sub( float *fPlanes, CUtlVector<CPolyhedron *> &WallBrushPolyhedrons_ClippedToWall, PS_InternalData_t &InternalData, CUtlVector<CPolyhedron *> &OutputPolyhedrons, float fFarRightPlaneDistance, float fFarLeftPlaneDistance, const Vector &vLeft, const Vector &vDown )
{
	const float fHalfHoleWidth = InternalData.Placement.fHalfWidth + PORTAL_HOLE_HALF_WIDTH_MOD;
	const float fHalfHoleHeight = InternalData.Placement.fHalfHeight + PORTAL_HOLE_HALF_HEIGHT_MOD;

	float *fSidePlanesOnly = &fPlanes[(2*4)];

	float fPlaneDistBackups[6];
	for( int i = 0; i != 6; ++i )
	{
		fPlaneDistBackups[i] = fPlanes[(i * 4) + 3];
	}

	CPolyhedron **pWallClippedPolyhedrons = NULL;
	int iWallClippedPolyhedronCount = 0;

	if( WallBrushPolyhedrons_ClippedToWall.Count() != 0 )
	{
		for( int i = WallBrushPolyhedrons_ClippedToWall.Count(); --i >= 0; )
		{
			CPolyhedron *pPolyhedron = ClipPolyhedron( WallBrushPolyhedrons_ClippedToWall[i], fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, true );
			if( pPolyhedron )
			{
				//a chunk of this brush passes through the hole, not eligible to be removed from cutting
				pPolyhedron->Release();
			}
			else
			{
				//no part of this brush interacts with the hole, no point in cutting the brush any later
				OutputPolyhedrons.AddToTail( WallBrushPolyhedrons_ClippedToWall[i] );
				WallBrushPolyhedrons_ClippedToWall.FastRemove( i );
			}
		}

		if( WallBrushPolyhedrons_ClippedToWall.Count() != 0 ) //might have become 0 while removing uncut brushes
		{
			pWallClippedPolyhedrons = WallBrushPolyhedrons_ClippedToWall.Base();
			iWallClippedPolyhedronCount = WallBrushPolyhedrons_ClippedToWall.Count();
		}
	}


	if( iWallClippedPolyhedronCount != 0 )
	{
		//upper wall
		{
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = InternalData.Placement.vUp.Dot( InternalData.Placement.ptCenter ) + (InternalData.Placement.fHalfHeight * 40.0f);
			fPlanes[(3*4) + 3] = vDown.Dot( InternalData.Placement.ptCenter ) - (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(4*4) + 3] = fFarLeftPlaneDistance;
			fPlanes[(5*4) + 3] = fFarRightPlaneDistance;			

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &OutputPolyhedrons );
		}

		//lower wall
		{
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = InternalData.Placement.vUp.Dot( InternalData.Placement.ptCenter ) - (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(3*4) + 3] = vDown.Dot( InternalData.Placement.ptCenter ) + (InternalData.Placement.fHalfHeight * 40.0f);
			fPlanes[(4*4) + 3] = fFarLeftPlaneDistance;
			fPlanes[(5*4) + 3] = fFarRightPlaneDistance;

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &OutputPolyhedrons );
		}

		//left wall
		{
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = InternalData.Placement.vUp.Dot( InternalData.Placement.ptCenter ) + (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(3*4) + 3] = vDown.Dot( InternalData.Placement.ptCenter ) + (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(4*4) + 3] = fFarLeftPlaneDistance;
			fPlanes[(5*4) + 3] = InternalData.Placement.vRight.Dot( InternalData.Placement.ptCenter ) - (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &OutputPolyhedrons );
		}

		//right wall
		{
			//fPlanes[(1*4) + 3] += 2000.0f;
			fPlanes[(2*4) + 3] = InternalData.Placement.vUp.Dot( InternalData.Placement.ptCenter ) + (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(3*4) + 3] = vDown.Dot( InternalData.Placement.ptCenter ) + (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(4*4) + 3] = vLeft.Dot( InternalData.Placement.ptCenter ) - (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);
			fPlanes[(5*4) + 3] = fFarRightPlaneDistance;

			ClipPolyhedrons( pWallClippedPolyhedrons, iWallClippedPolyhedronCount, fSidePlanesOnly, 4, PORTAL_POLYHEDRON_CUT_EPSILON, &OutputPolyhedrons );
		}
	}

	for( int i = 0; i != 6; ++i )
	{
		fPlanes[(i * 4) + 3] = fPlaneDistBackups[i];
	}
}


void CPortalSimulator::CreateTubePolyhedrons( void )
{
	Assert( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() == 0 );

	Vector vBackward = -m_InternalData.Placement.vForward;
	Vector vLeft = -m_InternalData.Placement.vRight;
	Vector vDown = -m_InternalData.Placement.vUp;

	const float fHalfHoleWidth = m_InternalData.Placement.fHalfWidth + PORTAL_HOLE_HALF_WIDTH_MOD;
	const float fHalfHoleHeight = m_InternalData.Placement.fHalfHeight + PORTAL_HOLE_HALF_HEIGHT_MOD;

	float fPlanes[6 * 4];

	float fTubeOffset = PORTAL_WALL_TUBE_OFFSET;

	if( m_InternalData.Placement.bParentIsVPhysicsSolidBrush )
	{
		fTubeOffset += VPHYSICS_SHRINK; //need to match VBSP shrinkage of brushes converted to physics models
	}

	//first and second planes are always forward and backward planes
	fPlanes[(0*4) + 0] = m_InternalData.Placement.vForward.x;
	fPlanes[(0*4) + 1] = m_InternalData.Placement.vForward.y;
	fPlanes[(0*4) + 2] = m_InternalData.Placement.vForward.z;
	fPlanes[(0*4) + 3] = m_InternalData.Placement.vForward.Dot( m_InternalData.Placement.ptCenter ) - fTubeOffset;

	fPlanes[(1*4) + 0] = vBackward.x;
	fPlanes[(1*4) + 1] = vBackward.y;
	fPlanes[(1*4) + 2] = vBackward.z;
	fPlanes[(1*4) + 3] = vBackward.Dot( m_InternalData.Placement.ptCenter ) + (PORTAL_WALL_TUBE_DEPTH + fTubeOffset);

	fPlanes[(2*4) + 0] = m_InternalData.Placement.vUp.x;
	fPlanes[(2*4) + 1] = m_InternalData.Placement.vUp.y;
	fPlanes[(2*4) + 2] = m_InternalData.Placement.vUp.z;
	fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;

	fPlanes[(3*4) + 0] = vDown.x;
	fPlanes[(3*4) + 1] = vDown.y;
	fPlanes[(3*4) + 2] = vDown.z;
	fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;

	fPlanes[(4*4) + 0] = vLeft.x;
	fPlanes[(4*4) + 1] = vLeft.y;
	fPlanes[(4*4) + 2] = vLeft.z;
	fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleWidth;

	fPlanes[(5*4) + 0] = m_InternalData.Placement.vRight.x;
	fPlanes[(5*4) + 1] = m_InternalData.Placement.vRight.y;
	fPlanes[(5*4) + 2] = m_InternalData.Placement.vRight.z;
	fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleWidth;



	//upper wall
	{
		//fPlanes[(1*4) + 3] = fTubeDepthDist;
		fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
		fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter ) - fHalfHoleHeight;
		fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);
		fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);

		CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pTubePolyhedron )
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );
	}

	//lower wall
	{
		//fPlanes[(1*4) + 3] = fTubeDepthDist;
		fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter ) - fHalfHoleHeight;
		fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleHeight + PORTAL_WALL_MIN_THICKNESS);
		fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);
		fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);

		CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pTubePolyhedron )
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );
	}

	//left wall
	{
		//fPlanes[(1*4) + 3] = fTubeDepthDist;
		fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;
		fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;
		fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);
		fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter ) - fHalfHoleWidth;

		CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pTubePolyhedron )
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );
	}

	//right wall
	{
		//minimal portion that extends into the hole space
		//fPlanes[(1*4) + 3] = fTubeDepthDist;
		fPlanes[(2*4) + 3] = m_InternalData.Placement.vUp.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;
		fPlanes[(3*4) + 3] = vDown.Dot( m_InternalData.Placement.ptCenter ) + fHalfHoleHeight;
		fPlanes[(4*4) + 3] = vLeft.Dot( m_InternalData.Placement.ptCenter ) - fHalfHoleWidth;
		fPlanes[(5*4) + 3] = m_InternalData.Placement.vRight.Dot( m_InternalData.Placement.ptCenter ) + (fHalfHoleWidth + PORTAL_WALL_MIN_THICKNESS);

		CPolyhedron *pTubePolyhedron = GeneratePolyhedronFromPlanes( fPlanes, 6, PORTAL_POLYHEDRON_CUT_EPSILON );
		if( pTubePolyhedron )
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.AddToTail( pTubePolyhedron );
	}
}



void CPortalSimulator::ClearPolyhedrons( void )
{
	if( m_CreationChecklist.bPolyhedronsGenerated == false )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearPolyhedrons() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	//world brushes
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() != 0 )
		{
			for( int i = m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.Count(); --i >= 0; )
				m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons[i]->Release();
			
			m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].Polyhedrons.RemoveAll();
		}
	}

	//world static props
	if( m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Static.World.StaticProps.Polyhedrons.RemoveAll();		
	}
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
	{
#ifndef CLIENT_DLL
		Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pPhysicsObject == NULL );
#endif
		Assert( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide == NULL );
	}
#endif
	m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.RemoveAll();

	//carved entities
	if( m_InternalData.Simulation.Dynamic.CarvedEntities.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Dynamic.CarvedEntities.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Dynamic.CarvedEntities.Polyhedrons.RemoveAll();

		for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
		{
			m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].CarvedPolyhedronGroup.iStartIndex = 0;
			m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].CarvedPolyhedronGroup.iNumPolyhedrons = 0;

			m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].UncarvedPolyhedronGroup.iStartIndex = 0;
			m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].UncarvedPolyhedronGroup.iNumPolyhedrons = 0;
		}
	}
#ifdef _DEBUG
	for( int i = m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
	{
#ifndef CLIENT_DLL
		Assert( m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pPhysicsObject == NULL );
#endif
		Assert( m_InternalData.Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pCollide == NULL );
	}
#endif

	//wall brushes
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.Count() != 0 )
		{
			for( int i = m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.Count(); --i >= 0; )
				m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons[i]->Release();

			m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].Polyhedrons.RemoveAll();
		}
	}

#if defined( GAME_DLL )
	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.Polyhedrons.RemoveAll();
	}
#endif

	//wall tube props
	if( m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count() != 0 )
	{
		for( int i = m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.Count(); --i >= 0; )
			m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons[i]->Release();

		m_InternalData.Simulation.Static.Wall.Local.Tube.Polyhedrons.RemoveAll();
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::ClearPolyhedrons() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	m_CreationChecklist.bPolyhedronsGenerated = false;
}


void CPortalSimulator::DebugCollisionOverlay( bool noDepthTest, float flDuration ) const
{
	if( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable )
	{
		UTIL_DebugOverlay_CPhysCollide( m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable, 255, 255, 255, noDepthTest, flDuration );
	}

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable )
		{
			UTIL_DebugOverlay_CPhysCollide( m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable, 0, 255, 0, noDepthTest, flDuration );
		}
	}

#if defined( GAME_DLL )
	if( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable )
	{
		UTIL_DebugOverlay_CPhysCollide( m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable, 255, 255, 0, noDepthTest, flDuration );
	}
#endif

	

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
		{
			UTIL_DebugOverlay_CPhysCollide( m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, 0, 255, 0, noDepthTest, flDuration );
		}
	}

	for( int i = 0; i != m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); ++i )
	{
		UTIL_DebugOverlay_CPhysCollide( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide, 0, 255, 255, noDepthTest, flDuration );
	}



	if( m_pLinkedPortal != NULL )
	{
		VMatrix linkedToThis = SetupMatrixOrgAngles( m_InternalData.Placement.ptaap_LinkedToThis.ptOriginTransform, m_InternalData.Placement.ptaap_LinkedToThis.qAngleTransform );

		if( m_pLinkedPortal->m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable )
		{
			UTIL_DebugOverlay_CPhysCollide( m_pLinkedPortal->m_InternalData.Simulation.Static.Wall.Local.Tube.pCollideable, 128, 128, 128, noDepthTest, flDuration, &linkedToThis.As3x4() );
		}

		/*if( m_pLinkedPortal->m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable )
		{
			UTIL_DebugOverlay_CPhysCollide( m_pLinkedPortal->m_InternalData.Simulation.Static.Wall.Local.Brushes.pCollideable, 255, 0, 0, noDepthTest, flDuration, &linkedToThis.As3x4() );
		}*/

		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_pLinkedPortal->m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
		{
			if( m_pLinkedPortal->m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
			{
				UTIL_DebugOverlay_CPhysCollide( m_pLinkedPortal->m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, 255, 0, 0, noDepthTest, flDuration, &linkedToThis.As3x4() );
			}
		}

		for( int i = 0; i != m_pLinkedPortal->m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); ++i )
		{
			UTIL_DebugOverlay_CPhysCollide( m_pLinkedPortal->m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide, 255, 0, 255, noDepthTest, flDuration, &linkedToThis.As3x4() );
		}
	}
}



void CPortalSimulator::DetachFromLinked( void )
{
	if( m_pLinkedPortal == NULL )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::DetachFromLinked() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	//IMPORTANT: Physics objects must be destroyed before their associated collision data or a fairly cryptic crash will ensue
#ifndef CLIENT_DLL
	ClearLinkedEntities();
	ClearLinkedPhysics();
#endif
	ClearLinkedCollision();

	if( m_pLinkedPortal->m_bInCrossLinkedFunction == false )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->DetachFromLinked();
		m_bInCrossLinkedFunction = false;
	}

	m_pLinkedPortal = NULL;

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::DetachFromLinked() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}

void CPortalSimulator::SetPortalSimulatorCallbacks( CPortalSimulatorEventCallbacks *pCallbacks )
{
	if( pCallbacks )
		m_pCallbacks = pCallbacks;
	else
		m_pCallbacks = &s_DummyPortalSimulatorCallback; //always keep the pointer valid
}



#ifndef CLIENT_DLL
void CPortalSimulator::SetVPhysicsSimulationEnabled( bool bEnabled )
{
	AssertMsg( (m_pLinkedPortal == NULL) || (m_pLinkedPortal->m_bSimulateVPhysics == m_bSimulateVPhysics), "Linked portals are in disagreement as to whether they would simulate VPhysics." );

	if( bEnabled == m_bSimulateVPhysics )
		return;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::SetVPhysicsSimulationEnabled() START\n", GetPortalSimulatorGUID(), TABSPACING ); );
	INCREMENTTABSPACING();
	
	m_bSimulateVPhysics = bEnabled;
	if( bEnabled )
	{
		//we took some local collision shortcuts when generating while physics simulation is off, regenerate
		ClearLocalCollision();
		ClearPolyhedrons();
		CreatePolyhedrons();
		CreateLocalCollision();
		CreateAllPhysics();
	}
	else
	{
		ClearAllPhysics();
	}

	if( m_pLinkedPortal && (m_pLinkedPortal->m_bInCrossLinkedFunction == false) )
	{
		Assert( m_bInCrossLinkedFunction == false ); //I'm pretty sure switching to a stack would have negative repercussions
		m_bInCrossLinkedFunction = true;
		m_pLinkedPortal->SetVPhysicsSimulationEnabled( bEnabled );
		m_bInCrossLinkedFunction = false;
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::SetVPhysicsSimulationEnabled() FINISH: %fms\n", GetPortalSimulatorGUID(), TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );
}
#endif


#ifndef CLIENT_DLL
void CPortalSimulator::PrePhysFrame( void )
{
	int iPortalSimulators = s_PortalSimulators.Count();

	if( iPortalSimulators != 0 )
	{
		CPortalSimulator **pAllSimulators = s_PortalSimulators.Base();
		for( int i = 0; i != iPortalSimulators; ++i )
		{
			CPortalSimulator *pSimulator = pAllSimulators[i];
			if( !pSimulator->IsReadyToSimulate() )
				continue;

			int iOwnedEntities = pSimulator->m_InternalData.Simulation.Dynamic.OwnedEntities.Count();
			if( iOwnedEntities != 0 )
			{
				CBaseEntity **pOwnedEntities = pSimulator->m_InternalData.Simulation.Dynamic.OwnedEntities.Base();

				for( int j = 0; j != iOwnedEntities; ++j )
				{
					CBaseEntity *pEntity = pOwnedEntities[j];
					if( CPhysicsShadowClone::IsShadowClone( pEntity ) )
						continue;

					Assert( (pEntity != NULL) && (pEntity->IsMarkedForDeletion() == false) );
					IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
					if( (pPhysObject == NULL) || pPhysObject->IsAsleep() )
						continue;

					int iEntIndex = pEntity->entindex();
					int iExistingFlags = pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex];
					if( pSimulator->EntityIsInPortalHole( pEntity ) )
						pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] |= PSEF_IS_IN_PORTAL_HOLE;
					else
						pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] &= ~PSEF_IS_IN_PORTAL_HOLE;

					UpdateShadowClonesPortalSimulationFlags( pEntity, PSEF_IS_IN_PORTAL_HOLE, pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] );

					if( ((iExistingFlags ^ pSimulator->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex]) & PSEF_IS_IN_PORTAL_HOLE) != 0 ) //value changed
					{
						pEntity->CollisionRulesChanged(); //entity moved into or out of the portal hole, need to either add or remove collision with transformed geometry

						CPhysicsShadowCloneLL *pClones = CPhysicsShadowClone::GetClonesOfEntity( pEntity );
						while( pClones )
						{
							pClones->pClone->CollisionRulesChanged();
							pClones = pClones->pNext;
						}
					}
				}
			}
		}
	}
}

void CPortalSimulator::PostPhysFrame( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player* pPlayer = (CPortal_Player *)UTIL_PlayerByIndex( i );
		if( pPlayer )
		{
			CPortal_Base2D* pTouchedPortal = pPlayer->m_hPortalEnvironment.Get();
			CPortalSimulator* pSim = GetSimulatorThatOwnsEntity( pPlayer );
			if ( pTouchedPortal && pSim && (pTouchedPortal->m_PortalSimulator.GetPortalSimulatorGUID() != pSim->GetPortalSimulatorGUID()) )
			{
				Warning ( "Player is simulated in a physics environment but isn't touching a portal! Can't teleport, but can fall through portal hole. Returning player to main environment.\n" );
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Player in PortalSimulator but not touching a portal, removing from sim at : %f\n",  gpGlobals->curtime ) );
				
				if ( pSim )
				{
					pSim->ReleaseOwnershipOfEntity( pPlayer, false );
				}
			}
		}
	}
}
#endif //#ifndef CLIENT_DLL

CPortalSimulator *CPortalSimulator::GetSimulatorThatOwnsEntity( const CBaseEntity *pEntity )
{
	int nEntIndex = pEntity->entindex();
	if( nEntIndex < 0 )
		return NULL;

#ifdef _DEBUG
	CPortalSimulator *pOwningSimulatorCheck = NULL;

	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		if( s_PortalSimulators[i]->m_InternalData.Simulation.Dynamic.EntFlags[nEntIndex] & PSEF_OWNS_ENTITY )
		{
			AssertMsg( pOwningSimulatorCheck == NULL, "More than one portal simulator found owning the same entity." );
			pOwningSimulatorCheck = s_PortalSimulators[i];
		}
	}

	AssertMsg( pOwningSimulatorCheck == s_OwnedEntityMap[nEntIndex], "Owned entity mapping out of sync with individual simulator ownership flags." );
#endif

	return s_OwnedEntityMap[nEntIndex];
}


#ifndef CLIENT_DLL
int CPortalSimulator::GetMoveableOwnedEntities( CBaseEntity **pEntsOut, int iEntOutLimit )
{
	int iOwnedEntCount = m_InternalData.Simulation.Dynamic.OwnedEntities.Count();
	int iOutputCount = 0;

	for( int i = 0; i != iOwnedEntCount; ++i )
	{
		CBaseEntity *pEnt = m_InternalData.Simulation.Dynamic.OwnedEntities[i];
		Assert( pEnt != NULL );

		if( CPhysicsShadowClone::IsShadowClone( pEnt ) )
			continue;

		if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEnt ) )
			continue;

		if( pEnt->GetMoveType() == MOVETYPE_NONE )
			continue;

        pEntsOut[iOutputCount] = pEnt;
		++iOutputCount;

		if( iOutputCount == iEntOutLimit )
			break;
	}

	return iOutputCount;
}



CPortalSimulator *CPortalSimulator::GetSimulatorThatCreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType )
{
	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		if( s_PortalSimulators[i]->CreatedPhysicsObject( pObject, pOut_SourceType ) )
			return s_PortalSimulators[i];
	}
	
	return NULL;
}

bool CPortalSimulator::CreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType ) const
{
	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		if( (pObject == m_InternalData.Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject) || (pObject == m_InternalData.Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject) )
		{
			if( pOut_SourceType )
				*pOut_SourceType = PSPOST_LOCAL_BRUSHES;

			return true;
		}
	}

#if defined( GAME_DLL )
	if( pObject == m_InternalData.Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject )
	{
		if( pOut_SourceType )
			*pOut_SourceType = PSPOST_LOCAL_BRUSHES;

		return true;
	}
#endif

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects ); ++iBrushSet )
	{
		if( pObject == m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] )
		{
			if( pOut_SourceType )
				*pOut_SourceType = PSPOST_REMOTE_BRUSHES;

			return true;
		}
	}

	for( int i = m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Static.World.StaticProps.ClippedRepresentations[i].pPhysicsObject == pObject )
		{
			if( pOut_SourceType )
				*pOut_SourceType = PSPOST_LOCAL_STATICPROPS;
			return true;
		}
	}

	for( int i = m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects.Count(); --i >= 0; )
	{
		if( m_InternalData.Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.PhysicsObjects[i] == pObject )
		{
			if( pOut_SourceType )
				*pOut_SourceType = PSPOST_REMOTE_STATICPROPS;

			return true;
		}
	}

	if( pObject == m_InternalData.Simulation.Static.Wall.Local.Tube.pPhysicsObject )
	{
		if( pOut_SourceType )
			*pOut_SourceType = PSPOST_HOLYWALL_TUBE;

		return true;
	}

	if( pObject == m_InternalData.Simulation.Static.World.Displacements.pPhysicsObject )
	{
		if( pOut_SourceType )
			*pOut_SourceType = PSPOST_LOCAL_DISPLACEMENT;

		return true;
	}

	return false;
}
#endif //#ifndef CLIENT_DLL








static void ConvertBrushListToClippedPolyhedronList( const uint32 *pBrushes, int iBrushCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList )
{
	if( pPolyhedronList == NULL )
		return;

	if( (pBrushes == NULL) || (iBrushCount == 0) )
		return;

	for( int i = 0; i != iBrushCount; ++i )
	{
		const CPolyhedron *pBrushPolyhedron = g_StaticCollisionPolyhedronCache.GetBrushPolyhedron( pBrushes[i] );
		CPolyhedron *pPolyhedron = ClipPolyhedron( pBrushPolyhedron, pOutwardFacingClipPlanes, iClipPlaneCount, fClipEpsilon );
		if( pPolyhedron )
		{
			pPolyhedronList->AddToTail( pPolyhedron );
		}

		g_StaticCollisionPolyhedronCache.ReleaseBrushPolyhedron( pBrushes[i], pBrushPolyhedron );
	}
}

static void ClipPolyhedrons( CPolyhedron * const *pExistingPolyhedrons, int iPolyhedronCount, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fClipEpsilon, CUtlVector<CPolyhedron *> *pPolyhedronList )
{
	if( pPolyhedronList == NULL )
		return;

	if( (pExistingPolyhedrons == NULL) || (iPolyhedronCount == 0) )
		return;

	for( int i = 0; i != iPolyhedronCount; ++i )
	{
		CPolyhedron *pPolyhedron = ClipPolyhedron( pExistingPolyhedrons[i], pOutwardFacingClipPlanes, iClipPlaneCount, fClipEpsilon );
		if( pPolyhedron )
			pPolyhedronList->AddToTail( pPolyhedron );
	}
}

//#define DUMP_POLYHEDRON_BEFORE_CONVERSION //uncomment to enable code that dumps each polyhedron just before it converts to a CPhysConvex (a very common place to crash if anything is amiss with the polyhedron).

static CPhysCollide *ConvertPolyhedronsToCollideable( CPolyhedron **pPolyhedrons, int iPolyhedronCount )
{
	if( (pPolyhedrons == NULL) || (iPolyhedronCount == 0 ) )
		return NULL;

	CREATEDEBUGTIMER( functionTimer );

	STARTDEBUGTIMER( functionTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sConvertPolyhedronsToCollideable() START\n", s_iPortalSimulatorGUID, TABSPACING ); );
	INCREMENTTABSPACING();

	CPhysConvex **pConvexes = (CPhysConvex **)stackalloc( iPolyhedronCount * sizeof( CPhysConvex * ) );
	int iConvexCount = 0;

#ifdef DUMP_POLYHEDRON_BEFORE_CONVERSION
	VMatrix matScaleNearOrigin;
	matScaleNearOrigin.Identity();
	const float cScale = 10.0f;
	matScaleNearOrigin = matScaleNearOrigin.Scale( Vector( cScale, cScale, cScale ) );
#endif
	CREATEDEBUGTIMER( convexTimer );
	STARTDEBUGTIMER( convexTimer );
	for( int i = 0; i != iPolyhedronCount; ++i )
	{

#ifdef DUMP_POLYHEDRON_BEFORE_CONVERSION
		{
			matScaleNearOrigin.SetTranslation( -pPolyhedrons[i]->Center() * cScale );
#ifndef CLIENT_DLL
			const char *szDumpFile = "PolyConvertServer.txt";
#else
			const char *szDumpFile = "PolyConvertClient.txt";
#endif
			DumpPolyhedronToGLView( pPolyhedrons[i], szDumpFile, &matScaleNearOrigin, "wb" );
		}
#endif
		pConvexes[iConvexCount] = physcollision->ConvexFromConvexPolyhedron( *pPolyhedrons[i] );

		Assert( pConvexes[iConvexCount] != NULL );
		
		if( pConvexes[iConvexCount] )
			++iConvexCount;		
	}
	STOPDEBUGTIMER( convexTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sConvex Generation:%fms\n", s_iPortalSimulatorGUID, TABSPACING, convexTimer.GetDuration().GetMillisecondsF() ); );


	CPhysCollide *pReturn;
	if( iConvexCount != 0 )
	{
		CREATEDEBUGTIMER( collideTimer );
		STARTDEBUGTIMER( collideTimer );
		convertconvexparams_t params;
		params.Defaults();
		params.buildOptimizedTraceTables = true;
		params.bUseFastApproximateInertiaTensor = true;
		params.bBuildAABBTree = true;
		pReturn = physcollision->ConvertConvexToCollideParams( pConvexes, iConvexCount, params );
		STOPDEBUGTIMER( collideTimer );
		DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCollideable Generation:%fms\n", s_iPortalSimulatorGUID, TABSPACING, collideTimer.GetDuration().GetMillisecondsF() ); );
	}
	else
	{
		pReturn = NULL;
	}

	STOPDEBUGTIMER( functionTimer );
	DECREMENTTABSPACING();
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sConvertPolyhedronsToCollideable() FINISH: %fms\n", s_iPortalSimulatorGUID, TABSPACING, functionTimer.GetDuration().GetMillisecondsF() ); );

	return pReturn;
}


static inline CPolyhedron *TransformAndClipSinglePolyhedron( CPolyhedron *pExistingPolyhedron, const VMatrix &Transform, const float *pOutwardFacingClipPlanes, int iClipPlaneCount, float fCutEpsilon, bool bUseTempMemory )
{
	Vector *pTempPointArray = (Vector *)stackalloc( sizeof( Vector ) * pExistingPolyhedron->iVertexCount );
	Polyhedron_IndexedPolygon_t *pTempPolygonArray = (Polyhedron_IndexedPolygon_t *)stackalloc( sizeof( Polyhedron_IndexedPolygon_t ) * pExistingPolyhedron->iPolygonCount );

	Polyhedron_IndexedPolygon_t *pOriginalPolygons = pExistingPolyhedron->pPolygons;
	pExistingPolyhedron->pPolygons = pTempPolygonArray;

	Vector *pOriginalPoints = pExistingPolyhedron->pVertices;
	pExistingPolyhedron->pVertices = pTempPointArray;

	for( int j = 0; j != pExistingPolyhedron->iPolygonCount; ++j )
	{
		pTempPolygonArray[j].iFirstIndex = pOriginalPolygons[j].iFirstIndex;
		pTempPolygonArray[j].iIndexCount = pOriginalPolygons[j].iIndexCount;
		pTempPolygonArray[j].polyNormal = Transform.ApplyRotation( pOriginalPolygons[j].polyNormal );
	}

	for( int j = 0; j != pExistingPolyhedron->iVertexCount; ++j )
	{
		pTempPointArray[j] = Transform * pOriginalPoints[j];
	}

	CPolyhedron *pNewPolyhedron = ClipPolyhedron( pExistingPolyhedron, pOutwardFacingClipPlanes, iClipPlaneCount, fCutEpsilon, bUseTempMemory ); //copy the polyhedron

	//restore the original polyhedron to its former self
	pExistingPolyhedron->pVertices = pOriginalPoints;
	pExistingPolyhedron->pPolygons = pOriginalPolygons;

	return pNewPolyhedron;
}

static int GetEntityPhysicsObjects( IPhysicsEnvironment *pEnvironment, CBaseEntity *pEntity, IPhysicsObject **pRetList, int iRetListArraySize )
{	
	int iCount, iRetCount = 0;
	const IPhysicsObject **pList = pEnvironment->GetObjectList( &iCount );

	if( iCount > iRetListArraySize )
		iCount = iRetListArraySize;

	for ( int i = 0; i < iCount; ++i )
	{
		CBaseEntity *pEnvEntity = reinterpret_cast<CBaseEntity *>(pList[i]->GetGameData());
		if ( pEntity == pEnvEntity )
		{
			pRetList[iRetCount] = (IPhysicsObject *)(pList[i]);
			++iRetCount;
		}
	}

	return iRetCount;
}



#ifndef CLIENT_DLL
//Move all entities back to the main environment for removal, and make sure the main environment is in control during the UTIL_Remove process
struct UTIL_Remove_PhysicsStack_t
{
	IPhysicsEnvironment *pPhysicsEnvironment;
	CEntityList *pShadowList;
};
static CUtlVector<UTIL_Remove_PhysicsStack_t> s_UTIL_Remove_PhysicsStack;

void CPortalSimulator::Pre_UTIL_Remove( CBaseEntity *pEntity )
{
	int index = s_UTIL_Remove_PhysicsStack.AddToTail();
	s_UTIL_Remove_PhysicsStack[index].pPhysicsEnvironment = physenv;
	s_UTIL_Remove_PhysicsStack[index].pShadowList = g_pShadowEntities;
	int iEntIndex = pEntity->entindex();

	//NDebugOverlay::EntityBounds( pEntity, 0, 0, 0, 50, 5.0f );

	if( (CPhysicsShadowClone::IsShadowClone( pEntity ) == false) &&
		(CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntity ) == false) )
	{
		CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
		if( pOwningSimulator )
		{
			pOwningSimulator->ReleasePhysicsOwnership( pEntity, false );
			pOwningSimulator->ReleaseOwnershipOfEntity( pEntity );
		}

		//might be cloned from main to a few environments
		for( int i = s_PortalSimulators.Count(); --i >= 0; )
		{
			s_PortalSimulators[i]->StopCloningEntityFromMain( pEntity );
			s_PortalSimulators[i]->StopCloningEntityAcrossPortals( pEntity );
		}
	}

	for( int i = s_PortalSimulators.Count(); --i >= 0; )
	{
		s_PortalSimulators[i]->m_InternalData.Simulation.Dynamic.EntFlags[iEntIndex] = 0;
	}


	physenv = physenv_main;
	g_pShadowEntities = g_pShadowEntities_Main;
}

void CPortalSimulator::Post_UTIL_Remove( CBaseEntity *pEntity )
{
	int index = s_UTIL_Remove_PhysicsStack.Count() - 1;
	Assert( index >= 0 );
	UTIL_Remove_PhysicsStack_t &PhysicsStackEntry = s_UTIL_Remove_PhysicsStack[index];
	physenv = PhysicsStackEntry.pPhysicsEnvironment;
	g_pShadowEntities = PhysicsStackEntry.pShadowList;
	s_UTIL_Remove_PhysicsStack.FastRemove(index);

#ifdef _DEBUG
	for( int i = CPhysicsShadowClone::g_ShadowCloneList.Count(); --i >= 0; )
	{
		Assert( CPhysicsShadowClone::g_ShadowCloneList[i]->GetClonedEntity() != pEntity ); //shouldn't be any clones of this object anymore
	}
#endif
}

void UpdateShadowClonesPortalSimulationFlags( const CBaseEntity *pSourceEntity, unsigned int iFlags, int iSourceFlags )
{
	Assert( !CPhysicsShadowClone::IsShadowClone( pSourceEntity ) );
	unsigned int iOrFlags = iSourceFlags & iFlags;

	CPhysicsShadowCloneLL *pClones = CPhysicsShadowClone::GetClonesOfEntity( pSourceEntity );
	while( pClones )
	{
		CPhysicsShadowClone *pClone = pClones->pClone;
		CPortalSimulator *pCloneSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pClone );

		unsigned int *pFlags = (unsigned int *)&pCloneSimulator->GetInternalData().Simulation.Dynamic.EntFlags[pClone->entindex()];
		*pFlags &= ~iFlags;
		*pFlags |= iOrFlags;

		Assert( ((iSourceFlags ^ *pFlags) & iFlags) == 0 );

		pClones = pClones->pNext;
	}
}
#endif



#ifdef GAME_DLL
	class CPS_AutoGameSys_EntityListener : public CAutoGameSystem, public IEntityListener
#else
	class CPS_AutoGameSys_EntityListener : public CAutoGameSystem, public IClientEntityListener
#endif
{
public:
	virtual void LevelInitPreEntity( void )
	{
		for( int i = s_PortalSimulators.Count(); --i >= 0; )
			s_PortalSimulators[i]->ClearEverything();
	}

	virtual void LevelShutdownPreEntity( void )
	{
		for( int i = s_PortalSimulators.Count(); --i >= 0; )
			s_PortalSimulators[i]->ClearEverything();
	}

	virtual bool Init( void )
	{
#if defined( GAME_DLL )
		gEntList.AddListenerEntity( this );
#else
		ClientEntityList().AddListenerEntity( this );
#endif
		return true;
	}

	//virtual void OnEntityCreated( CBaseEntity *pEntity ) {}
	virtual void OnEntityDeleted( CBaseEntity *pEntity )
	{
#if defined( CLIENT_DLL )
		if( pEntity->entindex() < 0 )
			return;
#endif

		CPortalSimulator *pSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
		if( pSimulator )
		{
#if defined( GAME_DLL )
			pSimulator->ReleasePhysicsOwnership( pEntity, false );
#endif
			pSimulator->ReleaseOwnershipOfEntity( pEntity );
		}
		Assert( CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity ) == NULL );

		for( int i = s_PortalSimulators.Count(); --i >= 0; )
		{
#if defined( DBGFLAG_ASSERT )
			CPortalSimulator *pSimulator = s_PortalSimulators[i];
			for( int j = pSimulator->GetInternalData().Simulation.Dynamic.OwnedEntities.Count(); --j >= 0; )
			{
				Assert( pSimulator->GetInternalData().Simulation.Dynamic.OwnedEntities[j] != pEntity );
			}
#endif
			s_PortalSimulators[i]->ReleaseCarvedEntity( pEntity );
		}
	}
};
static CPS_AutoGameSys_EntityListener s_CPS_AGS_EL_Singleton;




#ifdef GAME_DLL
IMPLEMENT_SERVERCLASS_ST( CPSCollisionEntity, DT_PSCollisionEntity )
END_SEND_TABLE()
#else
IMPLEMENT_CLIENTCLASS_DT( CPSCollisionEntity, DT_PSCollisionEntity, CPSCollisionEntity )
END_RECV_TABLE()
#endif // ifdef GAME_DLL

LINK_ENTITY_TO_CLASS( portalsimulator_collisionentity, CPSCollisionEntity );

static bool s_PortalSimulatorCollisionEntities[MAX_EDICTS] = { false };

CPSCollisionEntity::CPSCollisionEntity( void )
#ifdef GAME_DLL
	: m_pOwningSimulator( NULL )
#endif
{
}

CPSCollisionEntity::~CPSCollisionEntity( void )
{
#ifdef GAME_DLL
	if( m_pOwningSimulator )
	{
		m_pOwningSimulator->m_InternalData.Simulation.Dynamic.EntFlags[entindex()] &= ~PSEF_OWNS_PHYSICS;
		m_pOwningSimulator->MarkAsReleased( this );
		m_pOwningSimulator->m_InternalData.Simulation.hCollisionEntity = NULL;
		m_pOwningSimulator = NULL;
	}
#endif
	s_PortalSimulatorCollisionEntities[entindex()] = false;
}

void CPSCollisionEntity::UpdateOnRemove( void )
{
	VPhysicsSetObject( NULL );

#ifdef GAME_DLL
	if( m_pOwningSimulator )
	{
		m_pOwningSimulator->m_InternalData.Simulation.Dynamic.EntFlags[entindex()] &= ~PSEF_OWNS_PHYSICS;
		m_pOwningSimulator->MarkAsReleased( this );
		m_pOwningSimulator->m_InternalData.Simulation.hCollisionEntity = NULL;
		m_pOwningSimulator = NULL;
	}
#endif

	s_PortalSimulatorCollisionEntities[entindex()] = false;

	BaseClass::UpdateOnRemove();
}

void CPSCollisionEntity::Spawn( void )
{
	BaseClass::Spawn();
	SetSolid( SOLID_CUSTOM );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );
	s_PortalSimulatorCollisionEntities[entindex()] = true;
	VPhysicsSetObject( NULL );
	AddFlag( FL_WORLDBRUSH );
	AddEffects( EF_NODRAW | EF_NOINTERP | EF_NOSHADOW | EF_NORECEIVESHADOW );
}

void CPSCollisionEntity::Activate( void )
{
	BaseClass::Activate();
	CollisionRulesChanged();
}

int CPSCollisionEntity::ObjectCaps( void )
{
	return ((BaseClass::ObjectCaps() | FCAP_DONT_SAVE) & ~(FCAP_FORCE_TRANSITION | FCAP_ACROSS_TRANSITION | FCAP_MUST_SPAWN | FCAP_SAVE_NON_NETWORKABLE));
}

bool CPSCollisionEntity::ShouldCollide( int collisionGroup, int contentsMask ) const
{
#ifdef GAME_DLL
	return GetWorldEntity()->ShouldCollide( collisionGroup, contentsMask );
#else
	return GetClientWorldEntity()->ShouldCollide( collisionGroup, contentsMask );
#endif
}

int CPSCollisionEntity::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
#ifdef GAME_DLL
	if( m_pOwningSimulator == NULL )
		return 0;

	if( (pList == NULL) || (listMax == 0) )
		return 0;

	int iRetVal = 0;

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_pOwningSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_pOwningSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject != NULL )
		{
			pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pPhysicsObject;
			++iRetVal;
			if( iRetVal == listMax )
				return iRetVal;
		}
	}

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		if( m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject != NULL )
		{
			pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pPhysicsObject;
			++iRetVal;
			if( iRetVal == listMax )
				return iRetVal;
		}
	}

	if( m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	if( m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Tube.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.Local.Tube.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects ); ++iBrushSet )
	{
		if( m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet] != NULL )
		{
			pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pPhysicsObjects[iBrushSet];
			++iRetVal;
			if( iRetVal == listMax )
				return iRetVal;
		}
	}

	if( m_pOwningSimulator->GetInternalData().Simulation.Static.World.Displacements.pPhysicsObject != NULL )
	{
		pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Static.World.Displacements.pPhysicsObject;
		++iRetVal;
		if( iRetVal == listMax )
			return iRetVal;
	}

	int iCarvedEntityCount = m_pOwningSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count();
	for( int i = 0; i != iCarvedEntityCount; ++i )
	{
		pList[iRetVal] = m_pOwningSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pPhysicsObject;
		if( pList[iRetVal] != NULL )
		{
			++iRetVal;
			if( iRetVal == listMax )
				return iRetVal;
		}
	}

	return iRetVal;
#else
	return 0;
#endif
}

bool CPSCollisionEntity::IsPortalSimulatorCollisionEntity( const CBaseEntity *pEntity )
{
	return (pEntity->entindex() < 0) ? false : s_PortalSimulatorCollisionEntities[pEntity->entindex()];
}

#ifdef CLIENT_DLL
void CPSCollisionEntity::UpdatePartitionListEntry() //make this trigger touchable on the client
{
	partition->RemoveAndInsert( 
		PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS | PARTITION_CLIENT_TRIGGER_ENTITIES | PARTITION_CLIENT_IK_ATTACHMENT,  // remove
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_STATIC_PROPS,  // add
		CollisionProp()->GetPartitionHandle() );
}
#endif







#ifdef DEBUG_PORTAL_COLLISION_ENVIRONMENTS

static void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );
static void PortalSimulatorDumps_DumpPlanesToGlView( float *pPlanes, int iPlaneCount, const char *pszFileName );
static void PortalSimulatorDumps_DumpBoxToGlView( const Vector &vMins, const Vector &vMaxs, float fRed, float fGreen, float fBlue, const char *pszFileName );
static void PortalSimulatorDumps_DumpOBBoxToGlView( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, float fRed, float fGreen, float fBlue, const char *pszFileName );

void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName )
{
	CREATEDEBUGTIMER( collisionDumpTimer );
	STARTDEBUGTIMER( collisionDumpTimer );
	
	//color coding scheme, static prop collision is brighter than brush collision. Remote world stuff transformed to the local wall is darker than completely local stuff
#define PSDAC_INTENSITY_LOCALBRUSH 0.5f
#define PSDAC_INTENSITY_LOCALPROP 0.75f
#define PSDAC_INTENSITY_REMOTEBRUSH 0.0625f
#define PSDAC_INTENSITY_REMOTEPROP 0.25f
#define PSDAC_INTENSITY_CARVEDENTITY 1.0f

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
	{
		if( pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
			PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );
	}
	
	if( pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.bCollisionExists )
	{
		for( int i = pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
		{
			Assert( pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide );
			PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALPROP, szFileName );	
		}
	}

	if( pPortalSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.bCollisionExists )
	{
		for( int i = pPortalSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.CarvedRepresentations.Count(); --i >= 0; )
		{
			if( pPortalSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pCollide )
			{
				ICollideable *pProp = pPortalSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pSourceEntity->GetCollideable();
				PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Dynamic.CarvedEntities.CarvedRepresentations[i].pCollide, pProp->GetCollisionOrigin(), pProp->GetCollisionAngles(), PSDAC_INTENSITY_CARVEDENTITY, szFileName );
			}
		}
	}

	if ( pPortalSimulator->GetInternalData().Simulation.Static.World.Displacements.pCollideable )
		PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.World.Displacements.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );

	for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets ); ++iBrushSet )
	{
		if( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable )
			PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[iBrushSet].pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );
	}

#if defined( GAME_DLL )
	if( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable )
		PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Brushes.Carved_func_clip_vphysics.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );
#endif

	if( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Tube.pCollideable )
		PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.Wall.Local.Tube.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_LOCALBRUSH, szFileName );

	//if( pPortalSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable )
	//	PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.Brushes.pCollideable, vec3_origin, vec3_angle, PSDAC_INTENSITY_REMOTEBRUSH, szFileName );
	CPortalSimulator *pLinkedPortal = pPortalSimulator->GetLinkedPortalSimulator();
	if( pLinkedPortal )
	{
		for( int iBrushSet = 0; iBrushSet != ARRAYSIZE( pLinkedPortal->GetInternalData().Simulation.Static.World.Brushes.BrushSets ); ++iBrushSet )
		{
			if( pLinkedPortal->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable )
				PortalSimulatorDumps_DumpCollideToGlView( pLinkedPortal->GetInternalData().Simulation.Static.World.Brushes.BrushSets[iBrushSet].pCollideable, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.ptShrinkAlignedOrigin, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform, PSDAC_INTENSITY_REMOTEBRUSH, szFileName );
		}

		if ( pLinkedPortal->GetInternalData().Simulation.Static.World.Displacements.pCollideable )
			PortalSimulatorDumps_DumpCollideToGlView( pLinkedPortal->GetInternalData().Simulation.Static.World.Displacements.pCollideable, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.ptShrinkAlignedOrigin, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform, PSDAC_INTENSITY_REMOTEBRUSH, szFileName );

		//for( int i = pPortalSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.Collideables.Count(); --i >= 0; )
		//	PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Simulation.Static.Wall.RemoteTransformedToLocal.StaticProps.Collideables[i], vec3_origin, vec3_angle, PSDAC_INTENSITY_REMOTEPROP, szFileName );	
		if( pLinkedPortal->GetInternalData().Simulation.Static.World.StaticProps.bCollisionExists )
		{
			for( int i = pLinkedPortal->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations.Count(); --i >= 0; )
			{
				Assert( pLinkedPortal->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide );
				PortalSimulatorDumps_DumpCollideToGlView( pLinkedPortal->GetInternalData().Simulation.Static.World.StaticProps.ClippedRepresentations[i].pCollide, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.ptShrinkAlignedOrigin, pPortalSimulator->GetInternalData().Placement.ptaap_LinkedToThis.qAngleTransform, PSDAC_INTENSITY_REMOTEPROP, szFileName );	
			}
		}
	}

	if( sv_dump_portalsimulator_holeshapes.GetBool() )
	{
		if( pPortalSimulator->GetInternalData().Placement.pHoleShapeCollideable )
			PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Placement.pHoleShapeCollideable, vec3_origin, vec3_angle, 0.2f, szFileName );

		if( pPortalSimulator->GetInternalData().Placement.pInvHoleShapeCollideable )
			PortalSimulatorDumps_DumpCollideToGlView( pPortalSimulator->GetInternalData().Placement.pInvHoleShapeCollideable, vec3_origin, vec3_angle, 0.1f, szFileName );
	}

	STOPDEBUGTIMER( collisionDumpTimer );
	DEBUGTIMERONLY( DevMsg( 2, "[PSDT:%d] %sCPortalSimulator::DumpActiveCollision() Spent %fms generating a collision dump\n", pPortalSimulator->GetPortalSimulatorGUID(), TABSPACING, collisionDumpTimer.GetDuration().GetMillisecondsF() ); );
}

static void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename )
{
	if ( !pCollide )
		return;

	printf("Writing %s...\n", pFilename );
	Vector *outVerts;
	int vertCount = physcollision->CreateDebugMesh( pCollide, &outVerts );
	FileHandle_t fp = filesystem->Open( pFilename, "ab" );
	int triCount = vertCount / 3;
	int vert = 0;
	VMatrix tmp = SetupMatrixOrgAngles( origin, angles );
	int i;
	for ( i = 0; i < vertCount; i++ )
	{
		outVerts[i] = tmp.VMul4x3( outVerts[i] );
	}

	for ( i = 0; i < triCount; i++ )
	{
		filesystem->FPrintf( fp, "3\n" );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f 0 0\n", outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fColorScale );
		vert++;
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f 0 %.2f 0\n", outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fColorScale );
		vert++;
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f 0 0 %.2f\n", outVerts[vert].x, outVerts[vert].y, outVerts[vert].z, fColorScale );
		vert++;
	}
	filesystem->Close( fp );
	physcollision->DestroyDebugMesh( vertCount, outVerts );
}

static void PortalSimulatorDumps_DumpPlanesToGlView( float *pPlanes, int iPlaneCount, const char *pszFileName )
{
	FileHandle_t fp = filesystem->Open( pszFileName, "wb" );

	for( int i = 0; i < iPlaneCount; ++i )
	{
		Vector vPlaneVerts[4];

		float fRed, fGreen, fBlue;
		fRed = rand()/32768.0f;
		fGreen = rand()/32768.0f;
		fBlue = rand()/32768.0f;

		PolyFromPlane( vPlaneVerts, *(Vector *)(pPlanes + (i*4)), pPlanes[(i*4) + 3], 1000.0f );

		filesystem->FPrintf( fp, "4\n" );

		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[3].x, vPlaneVerts[3].y, vPlaneVerts[3].z, fRed, fGreen, fBlue );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[2].x, vPlaneVerts[2].y, vPlaneVerts[2].z, fRed, fGreen, fBlue );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[1].x, vPlaneVerts[1].y, vPlaneVerts[1].z, fRed, fGreen, fBlue );
		filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vPlaneVerts[0].x, vPlaneVerts[0].y, vPlaneVerts[0].z, fRed, fGreen, fBlue );
	}

	filesystem->Close( fp );
}


static void PortalSimulatorDumps_DumpBoxToGlView( const Vector &vMins, const Vector &vMaxs, float fRed, float fGreen, float fBlue, const char *pszFileName )
{
	FileHandle_t fp = filesystem->Open( pszFileName, "ab" );

	//x min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );

	//x max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );


	//y min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );



	//y max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );



	//z min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMins.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMins.z, fRed, fGreen, fBlue );



	//z max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMaxs.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMaxs.y, vMaxs.z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", vMins.x, vMins.y, vMaxs.z, fRed, fGreen, fBlue );

	filesystem->Close( fp );
}

static void PortalSimulatorDumps_DumpOBBoxToGlView( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, float fRed, float fGreen, float fBlue, const char *pszFileName )
{
	FileHandle_t fp = filesystem->Open( pszFileName, "ab" );

	Vector ptExtents[8];
	int counter;
	for( counter = 0; counter != 8; ++counter )
	{
		ptExtents[counter] = ptOrigin;
		if( counter & (1<<0) ) ptExtents[counter] += vExtent1;
		if( counter & (1<<1) ) ptExtents[counter] += vExtent2;
		if( counter & (1<<2) ) ptExtents[counter] += vExtent3;
	}

	//x min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );

	//x max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );


	//y min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );



	//y max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );



	//z min side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[1].x, ptExtents[1].y, ptExtents[1].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[3].x, ptExtents[3].y, ptExtents[3].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[2].x, ptExtents[2].y, ptExtents[2].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[0].x, ptExtents[0].y, ptExtents[0].z, fRed, fGreen, fBlue );



	//z max side
	filesystem->FPrintf( fp, "4\n" );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );

	filesystem->FPrintf( fp, "4\n" );	
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[5].x, ptExtents[5].y, ptExtents[5].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[7].x, ptExtents[7].y, ptExtents[7].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[6].x, ptExtents[6].y, ptExtents[6].z, fRed, fGreen, fBlue );
	filesystem->FPrintf( fp, "%6.3f %6.3f %6.3f %.2f %.2f %.2f\n", ptExtents[4].x, ptExtents[4].y, ptExtents[4].z, fRed, fGreen, fBlue );

	filesystem->Close( fp );
}



#endif














