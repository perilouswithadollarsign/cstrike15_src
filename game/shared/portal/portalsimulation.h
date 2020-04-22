//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides structures and classes necessary to simulate a portal.
//
// $NoKeywords: $
//=====================================================================================//

#ifndef PORTALSIMULATION_H
#define PORTALSIMULATION_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/polyhedron.h"
#include "const.h"
#include "tier1/utlmap.h"
#include "tier1/utlvector.h"

#define PORTAL_SIMULATORS_EMBED_GUID //define this to embed a unique integer with each portal simulator for debugging purposes

struct PropPolyhedronGroup_t //each static prop is made up of a group of polyhedrons, these help us pull those groups from an array
{
	int iStartIndex;
	int iNumPolyhedrons;
};

enum PortalSimulationEntityFlags_t
{
	PSEF_OWNS_ENTITY = (1 << 0), //this environment is responsible for the entity's physics objects
	PSEF_OWNS_PHYSICS = (1 << 1),
	PSEF_IS_IN_PORTAL_HOLE = (1 << 2), //updated per-phyframe
	PSEF_CLONES_ENTITY_FROM_MAIN = (1 << 3), //entity is close enough to the portal to affect objects intersecting the portal
	PSEF_CLONES_ENTITY_ACROSS_PORTAL_FROM_MAIN = (1 << 4), //the entity is not "owned" by the portal, but creates a physics clone across the portal anyway
	//PSEF_HAS_LINKED_CLONE = (1 << 1), //this environment has a clone of the entity which is transformed from its linked portal
};

enum PS_PhysicsObjectSourceType_t
{
	PSPOST_LOCAL_BRUSHES,
	PSPOST_REMOTE_BRUSHES,
	PSPOST_LOCAL_STATICPROPS,
	PSPOST_REMOTE_STATICPROPS,
	PSPOST_HOLYWALL_TUBE,
	PSPOST_LOCAL_DISPLACEMENT,
};

enum RayInPortalHoleResult_t
{
	RIPHR_NOT_TOUCHING_HOLE = 0,
	RIPHR_TOUCHING_HOLE_NOT_WALL, //only the hole
	RIPHR_TOUCHING_HOLE_AND_WALL, //both hole and surrounding wall
};

struct PortalTransformAsAngledPosition_t //a matrix transformation from this portal to the linked portal, stored as vector and angle transforms
{
	Vector ptOriginTransform;
	QAngle qAngleTransform;

	Vector ptShrinkAlignedOrigin; //when there's a discrepancy between visual surface and collision surface, this is adjusted to compensate in traces
};

inline bool LessFunc_Integer( const int &a, const int &b ) { return a < b; };


class CPortalSimulatorEventCallbacks //sends out notifications of events to game specific code
{
public:
	virtual void PortalSimulator_TookOwnershipOfEntity( CBaseEntity *pEntity ) { };
	virtual void PortalSimulator_ReleasedOwnershipOfEntity( CBaseEntity *pEntity ) { };

	virtual void PortalSimulator_TookPhysicsOwnershipOfEntity( CBaseEntity *pEntity ) { };
	virtual void PortalSimulator_ReleasedPhysicsOwnershipOfEntity( CBaseEntity *pEntity ) { };
};

//====================================================================================
// To any coder trying to understand the following nested structures....
//
// You may be wondering... why? wtf?
//
// The answer. The previous incarnation of server side portal simulation suffered
// terribly from evolving variables with increasingly cryptic names with no clear
// definition of what part of the system the variable was involved with.
//
// It's my hope that a nested structure with clear boundaries will eliminate that 
// horrible, awful, nasty, frustrating confusion. (It was really really bad). This
// system has the added benefit of pseudo-forcing a naming structure.
//
// Lastly, if it all roots in one struct, we can const reference it out to allow 
// easy reads without writes
//
// It's broken out like this to solve a few problems....
// 1. It cleans up intellisense when you don't actually define a structure
//		within a structure.
// 2. Shorter typenames when you want to have a pointer/reference deep within
//		the nested structure.
// 3. Needed at least one level removed from CPortalSimulator so
//		pointers/references could be made while the primary instance of the
//		data was private/protected.
//
// It may be slightly difficult to understand in it's broken out structure, but
// intellisense brings all the data together in a very cohesive manner for
// working with.
//====================================================================================

struct PS_PlacementData_t //stuff useful for geometric operations
{
	Vector ptCenter;
	QAngle qAngles;
	Vector vForward;
	Vector vUp;
	Vector vRight;
	float fHalfWidth, fHalfHeight;
	VPlane PortalPlane;
	VMatrix matThisToLinked;
	VMatrix matLinkedToThis;
	PortalTransformAsAngledPosition_t ptaap_ThisToLinked;
	PortalTransformAsAngledPosition_t ptaap_LinkedToThis;
	CPhysCollide *pHoleShapeCollideable; //used to test if a collideable is in the hole, should NOT be collided against in general
	CPhysCollide *pInvHoleShapeCollideable; //A very thin, but wide wall with the portal hole cut out in the middle. Used to test if traces are fully encapsulated in a portal hole
	CPhysCollide *pAABBAngleTransformCollideable; //used for player traces so we can slide into the portal gracefully if there's an angular difference such that our transformed AABB is in solid until the center reaches the plane
	Vector vecCurAABBMins;
	Vector vecCurAABBMaxs;
	Vector vCollisionCloneExtents; //how far in each direction (in front of the portal) we clone collision data from the real world.
	EHANDLE hPortalPlacementParent;
	bool bParentIsVPhysicsSolidBrush; //VPhysics solid brushes present an interesting collision challenge where their visuals are separated 0.5 inches from their collision
	PS_PlacementData_t( void )
	{
		memset( this, 0, sizeof( PS_PlacementData_t ) );
		ptCenter.Invalidate();
	}
};

struct PS_SD_Static_CarvedBrushCollection_t
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CPhysCollide *pCollideable;
#ifndef CLIENT_DLL
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_CarvedBrushCollection_t() : pCollideable(NULL), pPhysicsObject(NULL) {};
#else
	PS_SD_Static_CarvedBrushCollection_t() : pCollideable(NULL) {};
#endif
};

struct PS_SD_Static_BrushSet_t : public PS_SD_Static_CarvedBrushCollection_t
{
	PS_SD_Static_BrushSet_t() : iSolidMask(0) {};
	int iSolidMask;
};

struct PS_SD_Static_World_Brushes_t
{
	PS_SD_Static_BrushSet_t BrushSets[4];

	PS_SD_Static_World_Brushes_t()
	{
		BrushSets[0].iSolidMask = MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE;
		BrushSets[1].iSolidMask = CONTENTS_GRATE;
		BrushSets[2].iSolidMask = CONTENTS_PLAYERCLIP;
		BrushSets[3].iSolidMask = CONTENTS_MONSTERCLIP;
	}
};

struct PS_SD_Static_World_Displacements_t
{
	CPhysCollide *pCollideable;
#ifndef CLIENT_DLL
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_World_Displacements_t() : pCollideable(NULL), pPhysicsObject(NULL) {};
#else
	PS_SD_Static_World_Displacements_t() : pCollideable(NULL) {};
#endif
};


struct PS_SD_Static_World_StaticProps_ClippedProp_t
{
	PropPolyhedronGroup_t			PolyhedronGroup;
	CPhysCollide *					pCollide;
#ifndef CLIENT_DLL
	IPhysicsObject *				pPhysicsObject;
#endif
	IHandleEntity *					pSourceProp;

	int								iTraceContents;
	short							iTraceSurfaceProps;
	static CBaseEntity *			pTraceEntity;
	static const char *				szTraceSurfaceName; //same for all static props, here just for easy reference
	static const int				iTraceSurfaceFlags; //same for all static props, here just for easy reference
};

struct PS_SD_Static_World_StaticProps_t
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CUtlVector<PS_SD_Static_World_StaticProps_ClippedProp_t> ClippedRepresentations;
	bool bCollisionExists; //the shortcut to know if collideables exist for each prop
#ifndef CLIENT_DLL
	bool bPhysicsExists; //the shortcut to know if physics obects exist for each prop
#endif
	PS_SD_Static_World_StaticProps_t( void ) : bCollisionExists( false )
#ifndef CLIENT_DLL
		, bPhysicsExists( false )
#endif
	{ };
};

struct PS_SD_Static_World_t //stuff in front of the portal
{
	PS_SD_Static_World_Brushes_t Brushes;
	PS_SD_Static_World_Displacements_t Displacements;
	PS_SD_Static_World_StaticProps_t StaticProps;
};

struct PS_SD_Static_Wall_Local_Tube_t //a minimal tube, an object must fit inside this to be eligible for portaling
{
	CUtlVector<CPolyhedron *> Polyhedrons; //the building blocks of more complex collision
	CPhysCollide *pCollideable;

#ifndef CLIENT_DLL
	IPhysicsObject *pPhysicsObject;
	PS_SD_Static_Wall_Local_Tube_t() : pCollideable(NULL), pPhysicsObject(NULL) {};
#else
	PS_SD_Static_Wall_Local_Tube_t() : pCollideable(NULL) {};
#endif
};

struct PS_SD_Static_Wall_Local_Brushes_t 
{
	PS_SD_Static_BrushSet_t BrushSets[4];
#if defined( GAME_DLL )
	PS_SD_Static_CarvedBrushCollection_t Carved_func_clip_vphysics; //physics only, no tracing
#endif

	PS_SD_Static_Wall_Local_Brushes_t()
	{
		BrushSets[0].iSolidMask = MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE;
		BrushSets[1].iSolidMask = CONTENTS_GRATE;
		BrushSets[2].iSolidMask = CONTENTS_PLAYERCLIP;
		BrushSets[3].iSolidMask = CONTENTS_MONSTERCLIP;
	}
};

struct PS_SD_Static_Wall_Local_t //things in the wall that are completely independant of having a linked portal
{
	PS_SD_Static_Wall_Local_Tube_t Tube;
	PS_SD_Static_Wall_Local_Brushes_t Brushes;
};

struct PS_SD_Static_Wall_RemoteTransformedToLocal_Brushes_t
{
	IPhysicsObject *pPhysicsObjects[ARRAYSIZE(((PS_SD_Static_World_Brushes_t *)NULL)->BrushSets)];
	PS_SD_Static_Wall_RemoteTransformedToLocal_Brushes_t()
	{
		for( int i = 0; i != ARRAYSIZE(pPhysicsObjects); ++i )
		{
			pPhysicsObjects[i] = NULL;
		}
	};
};

struct PS_SD_Static_Wall_RemoteTransformedToLocal_StaticProps_t
{
	CUtlVector<IPhysicsObject *> PhysicsObjects;
};

struct PS_SD_Static_Wall_RemoteTransformedToLocal_t //things taken from the linked portal's "World" collision and transformed into local space
{
	PS_SD_Static_Wall_RemoteTransformedToLocal_Brushes_t Brushes;
	PS_SD_Static_Wall_RemoteTransformedToLocal_StaticProps_t StaticProps;
};

struct PS_SD_Static_Wall_t //stuff behind the portal
{
	PS_SD_Static_Wall_Local_t Local;
#ifndef CLIENT_DLL
	PS_SD_Static_Wall_RemoteTransformedToLocal_t RemoteTransformedToLocal;
#endif
};

struct PS_SD_Static_SurfaceProperties_t //surface properties to pretend every collideable here is using
{
	int contents;
	csurface_t surface;
	CBaseEntity *pEntity;
};

struct PS_SD_Static_t //stuff that doesn't move around
{
	PS_SD_Static_World_t World;
	PS_SD_Static_Wall_t Wall;
	PS_SD_Static_SurfaceProperties_t SurfaceProperties;
};

class CPhysicsShadowClone;

struct PS_SD_Dynamic_PhysicsShadowClones_t
{
	CUtlVector<CBaseEntity *> ShouldCloneFromMain; //a list of entities that should be cloned from main if physics simulation is enabled
													//in single-environment mode, this helps us track who should collide with who
	
	CUtlVector<CPhysicsShadowClone *> FromLinkedPortal;

	CUtlVector<CBaseEntity *> ShouldCloneToRemotePortal; //non-owned entities that we should push a clone for
};


struct PS_SD_Dynamic_CarvedEntities_CarvedEntity_t
{
	PropPolyhedronGroup_t			UncarvedPolyhedronGroup;
	PropPolyhedronGroup_t			CarvedPolyhedronGroup;
	CPhysCollide *					pCollide;
#ifndef CLIENT_DLL
	IPhysicsObject *				pPhysicsObject;
#endif
	CBaseEntity *					pSourceEntity;
};

struct PS_SD_Dynamic_CarvedEntities_t
{
	bool bCollisionExists; //the shortcut to know if collideables exist for each entity
#ifndef CLIENT_DLL
	bool bPhysicsExists; //the shortcut to know if physics obects exist for each entity
#endif
	CUtlVector<CPolyhedron *> Polyhedrons;
	CUtlVector<PS_SD_Dynamic_CarvedEntities_CarvedEntity_t> CarvedRepresentations;

	PS_SD_Dynamic_CarvedEntities_t( void ) : bCollisionExists( false )
#ifndef CLIENT_DLL
		, bPhysicsExists( false )
#endif
	{ };
};

struct PS_SD_Dynamic_t //stuff that moves around
{
	unsigned int EntFlags[MAX_EDICTS]; //flags maintained for every entity in the world based on its index
	CUtlVector<CBaseEntity *> OwnedEntities;	

#ifndef CLIENT_DLL
	PS_SD_Dynamic_PhysicsShadowClones_t ShadowClones;
#endif

	uint32 HasCarvedVersionOfEntity[(MAX_EDICTS + (sizeof(uint32) * 8) - 1)/(sizeof(uint32) * 8)]; //a bit for every possible ent index rounded up to the next integer, not stored as a PortalSimulationEntityFlags_t because those are all serverside at the moment

	PS_SD_Dynamic_CarvedEntities_t CarvedEntities;

	PS_SD_Dynamic_t()
	{
		memset( EntFlags, 0, sizeof( EntFlags ) );
		memset( HasCarvedVersionOfEntity, 0, sizeof( HasCarvedVersionOfEntity ) );
	}
};

class CPortalSimulator;

class CPSCollisionEntity : public CBaseEntity
{
	DECLARE_CLASS( CPSCollisionEntity, CBaseEntity );

#ifdef GAME_DLL
	DECLARE_SERVERCLASS();
#else
	DECLARE_CLIENTCLASS();
#endif

#ifdef GAME_DLL
private:
	CPortalSimulator *m_pOwningSimulator;
#endif

public:
	CPSCollisionEntity( void );
	virtual ~CPSCollisionEntity( void );

	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual int		ObjectCaps( void );
	virtual int		VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );
	virtual void	UpdateOnRemove( void );
	virtual	bool	ShouldCollide( int collisionGroup, int contentsMask ) const;


#ifdef GAME_DLL
	virtual void	VPhysicsCollision( int index, gamevcollisionevent_t *pEvent ) {}
	virtual void	VPhysicsFriction( IPhysicsObject *pObject, float energy, int surfaceProps, int surfacePropsHit ) {}
	virtual int UpdateTransmitState( void )	{ return SetTransmitState( FL_EDICT_ALWAYS );	}
#else
	virtual void UpdatePartitionListEntry(); //make this trigger touchable on the client
#endif

	static bool		IsPortalSimulatorCollisionEntity( const CBaseEntity *pEntity );
	friend class CPortalSimulator;
};

struct PS_SimulationData_t //compartmentalized data for coherent management
{
	DECLARE_CLASS_NOBASE( PS_SimulationData_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	PS_SD_Static_t Static;

	PS_SD_Dynamic_t Dynamic;

#ifndef CLIENT_DLL
	IPhysicsEnvironment *pPhysicsEnvironment;
	CNetworkHandle( CPSCollisionEntity, hCollisionEntity );

	PS_SimulationData_t() : pPhysicsEnvironment(NULL){ hCollisionEntity = NULL; }
#else
	typedef CHandle<CPSCollisionEntity> CollisionEntityHandle_t;
	CollisionEntityHandle_t hCollisionEntity; //the entity we'll be tying physics objects to for collision

	PS_SimulationData_t() : hCollisionEntity(NULL) {}
#endif
};

struct PS_DebuggingData_t
{
	Color overlayColor; //a good base color to use when showing overlays
};

#ifdef GAME_DLL
EXTERN_SEND_TABLE( DT_PS_SimulationData_t );
#else
EXTERN_RECV_TABLE( DT_PS_SimulationData_t );
#endif

struct PS_InternalData_t
{
	DECLARE_CLASS_NOBASE( PS_InternalData_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	PS_PlacementData_t Placement;

#ifdef GAME_DLL
	CNetworkVarEmbedded( PS_SimulationData_t, Simulation);
#else
	PS_SimulationData_t Simulation;
#endif

	PS_DebuggingData_t Debugging;
};

#ifdef GAME_DLL
EXTERN_SEND_TABLE( DT_PS_InternalData_t );
#else
EXTERN_RECV_TABLE( DT_PS_InternalData_t );
#endif


class CPortalSimulator
{
public:
	DECLARE_CLASS_NOBASE( CPortalSimulator );
	DECLARE_EMBEDDED_NETWORKVAR();

public:
	CPortalSimulator( void );
	~CPortalSimulator( void );

	void				SetSize( float fHalfWidth, float fHalfHeight );
	void				MoveTo( const Vector &ptCenter, const QAngle &angles );
	void				ClearEverything( void );

	void				AttachTo( CPortalSimulator *pLinkedPortalSimulator );
	void				DetachFromLinked( void ); //detach portals to sever the connection, saves work when planning on moving both portals
	CPortalSimulator	*GetLinkedPortalSimulator( void ) const;

	void				SetPortalSimulatorCallbacks( CPortalSimulatorEventCallbacks *pCallbacks );
	
	bool				IsReadyToSimulate( void ) const; //is active and linked to another portal
	
	void				SetCollisionGenerationEnabled( bool bEnabled ); //enable/disable collision generation for the hole in the wall, needed for proper vphysics simulation
	bool				IsCollisionGenerationEnabled( void ) const;

#ifndef CLIENT_DLL
	void				SetVPhysicsSimulationEnabled( bool bEnabled ); //enable/disable vphysics simulation. Will automatically update the linked portal to be the same
	bool				IsSimulatingVPhysics( void ) const; //this portal is setup to handle any physically simulated object, false means the portal is handling player movement only
#endif

	bool				EntityIsInPortalHole( CBaseEntity *pEntity ) const; //true if the entity is within the portal cutout bounds and crossing the plane. Not just *near* the portal
	bool				EntityHitBoxExtentIsInPortalHole( CBaseAnimating *pBaseAnimating, bool bUseCollisionAABB ) const; //true if the entity is within the portal cutout bounds and crossing the plane. Not just *near* the portal
	void				RemoveEntityFromPortalHole( CBaseEntity *pEntity ); //if the entity is in the portal hole, this forcibly moves it out by any means possible

	RayInPortalHoleResult_t IsRayInPortalHole( const Ray_t &ray ) const; //traces a ray against the same detector for EntityIsInPortalHole(), bias is towards false positives

#ifndef CLIENT_DLL
	int				GetMoveableOwnedEntities( CBaseEntity **pEntsOut, int iEntOutLimit ); //gets owned entities that aren't either world or static props. Excludes fake portal ents such as physics clones

	static CPortalSimulator *GetSimulatorThatCreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType = NULL );
	static void			Pre_UTIL_Remove( CBaseEntity *pEntity );
	static void			Post_UTIL_Remove( CBaseEntity *pEntity );

	//void				TeleportEntityToLinkedPortal( CBaseEntity *pEntity );
	void				StartCloningEntityFromMain( CBaseEntity *pEntity );
	void				StopCloningEntityFromMain( CBaseEntity *pEntity );

	//these 2 only apply for entities that this simulator will not take ownership of
	void				StartCloningEntityAcrossPortals( CBaseEntity *pEntity );
	void				StopCloningEntityAcrossPortals( CBaseEntity *pEntity );

	bool				OwnsPhysicsForEntity( const CBaseEntity *pEntity ) const;

	bool				CreatedPhysicsObject( const IPhysicsObject *pObject, PS_PhysicsObjectSourceType_t *pOut_SourceType = NULL ) const; //true if the physics object was generated by this portal simulator

	static void			PrePhysFrame( void );
	static void			PostPhysFrame( void );

#endif //#ifndef CLIENT_DLL

	//these three really should be made internal and the public interface changed to a "watch this entity" setup
	void				TakeOwnershipOfEntity( CBaseEntity *pEntity ); //general ownership, not necessarily physics ownership
	void				ReleaseOwnershipOfEntity( CBaseEntity *pEntity, bool bMovingToLinkedSimulator = false ); //if bMovingToLinkedSimulator is true, the code skips some steps that are going to be repeated when the entity is added to the other simulator
	void				ReleaseAllEntityOwnership( void ); //go back to not owning any entities

	bool				OwnsEntity( const CBaseEntity *pEntity ) const;

	static CPortalSimulator *GetSimulatorThatOwnsEntity( const CBaseEntity *pEntity ); //fairly cheap to call


	bool				IsEntityCarvedByPortal( int iEntIndex ) const;
	inline bool			IsEntityCarvedByPortal( CBaseEntity *pEntity ) const { return IsEntityCarvedByPortal( pEntity->entindex() ); };
	CPhysCollide *		GetCollideForCarvedEntity( CBaseEntity *pEntity ) const;

#ifdef PORTAL_SIMULATORS_EMBED_GUID
	int					GetPortalSimulatorGUID( void ) const { return m_iPortalSimulatorGUID; };
#endif

	void				SetCarvedParent( CBaseEntity *pPortalPlacementParent ); //sometimes you have to carve up a func brush the portal was placed on

	void				DebugCollisionOverlay( bool noDepthTest, float flDuration ) const;

protected:
	void				MovedOrResized( const Vector &ptCenter, const QAngle &qAngles, float fHalfWidth, float fHalfHeight ); //MoveTo() and SetSize() funnel here to create geometry
	void				AddCarvedEntity( CBaseEntity *pEntity ); //finds/adds an entity that we should carve with the portal hole
	void				ReleaseCarvedEntity( CBaseEntity *pEntity ); //finds and removes a carved entity
	bool				m_bLocalDataIsReady; //this side of the portal is properly setup, no guarantees as to linkage to another portal
	bool				m_bSimulateVPhysics;
	bool				m_bGenerateCollision;
	bool				m_bSharedCollisionConfiguration; //when portals are in certain configurations, they need to cross-clip and share some collision data and things get nasty. For the love of all that is holy, pray that this is false.
	CPortalSimulator	*m_pLinkedPortal;
	bool				m_bInCrossLinkedFunction; //A flag to mark that we're already in a linked function and that the linked portal shouldn't call our side
	CPortalSimulatorEventCallbacks *m_pCallbacks; 
#ifdef PORTAL_SIMULATORS_EMBED_GUID
	int					m_iPortalSimulatorGUID;
#endif

	struct
	{
		bool			bPolyhedronsGenerated;
		bool			bLocalCollisionGenerated;
		bool			bLinkedCollisionGenerated;
		bool			bLocalPhysicsGenerated;
		bool			bLinkedPhysicsGenerated;
	} m_CreationChecklist;

	friend class CPSCollisionEntity;

#ifndef CLIENT_DLL //physics handled purely by server side
	void				TakePhysicsOwnership( CBaseEntity *pEntity );
	void				ReleasePhysicsOwnership( CBaseEntity *pEntity, bool bContinuePhysicsCloning = true, bool bMovingToLinkedSimulator = false );

	void				CreateAllPhysics( void );
	void				CreateMinimumPhysics( void ); //stuff needed by any part of physics simulations
	void				CreateLocalPhysics( void );
	void				CreateLinkedPhysics( void );

	void				ClearAllPhysics( void );
	void				ClearMinimumPhysics( void );
	void				ClearLocalPhysics( void );
	void				ClearLinkedPhysics( void );

	void				ClearLinkedEntities( void ); //gets rid of transformed shadow clones
#endif

	void				CreateAllCollision( void );
	void				CreateLocalCollision( void );
	void				CreateLinkedCollision( void );

	void				ClearAllCollision( void );
	void				ClearLinkedCollision( void );
	void				ClearLocalCollision( void );

	void				CreatePolyhedrons( void ); //carves up the world around the portal's position into sets of polyhedrons
	void				ClearPolyhedrons( void );
	void				CreateTubePolyhedrons( void ); //Sometimes we have to shift the portal tube helper collideable around a bit

	void				UpdateLinkMatrix( void );

	void				MarkAsOwned( CBaseEntity *pEntity );
	void				MarkAsReleased( CBaseEntity *pEntity );

#ifdef GAME_DLL
	CNetworkVarEmbedded( PS_InternalData_t, m_InternalData );
#else
	PS_InternalData_t m_InternalData;
#endif

public:
	inline const PS_InternalData_t &GetInternalData() const;
	PS_DebuggingData_t &EditDebuggingData();

	friend class CPS_AutoGameSys_EntityListener;
};

#ifdef GAME_DLL
EXTERN_SEND_TABLE( DT_PortalSimulator );
#else
EXTERN_RECV_TABLE( DT_PortalSimulator );
#endif


extern CUtlVector<CPortalSimulator *> const &g_PortalSimulators;














inline bool CPortalSimulator::OwnsEntity( const CBaseEntity *pEntity ) const
{
	return ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_OWNS_ENTITY) != 0);
}

#ifndef CLIENT_DLL
inline bool CPortalSimulator::OwnsPhysicsForEntity( const CBaseEntity *pEntity ) const
{
	return ((m_InternalData.Simulation.Dynamic.EntFlags[pEntity->entindex()] & PSEF_OWNS_PHYSICS) != 0);
}
#endif

inline bool CPortalSimulator::IsReadyToSimulate( void ) const
{
	return m_bLocalDataIsReady && m_pLinkedPortal && m_pLinkedPortal->m_bLocalDataIsReady;
}

#ifndef CLIENT_DLL
inline bool CPortalSimulator::IsSimulatingVPhysics( void ) const
{
	return m_bSimulateVPhysics && m_bGenerateCollision;
}
#endif

inline bool CPortalSimulator::IsCollisionGenerationEnabled( void ) const
{
	return m_bGenerateCollision;
}

inline CPortalSimulator	*CPortalSimulator::GetLinkedPortalSimulator( void ) const
{
	return m_pLinkedPortal;
}

inline const PS_InternalData_t &CPortalSimulator::GetInternalData() const
{
	return m_InternalData;
}

inline PS_DebuggingData_t &CPortalSimulator::EditDebuggingData()
{
	return m_InternalData.Debugging;
}
#if defined ( GAME_DLL )
struct VPhysicsClipEntry_t
{
	EHANDLE hEnt;
	Vector vAABBMins;
	Vector vAABBMaxs;
};
CUtlVector<VPhysicsClipEntry_t>& GetVPhysicsClipList ( void );

#endif // GAME_DLL

#endif //#ifndef PORTALSIMULATION_H

