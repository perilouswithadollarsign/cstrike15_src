//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef RUBIKON_INTERSECTION_ATTRIBUTES_HDR
#define RUBIKON_INTERSECTION_ATTRIBUTES_HDR

static const uint32 INTERSECTION_ENTITY_ID_NONE = 0xFFFFFFFF;

enum BuiltInCollisionGroup_t
{
	// Default layer, always collides with everything.
	COLLISION_GROUP_ALWAYS            = 0, // "always"
	// This is how you turn off all collisions for an object - move it to this group
	COLLISION_GROUP_NONPHYSICAL	   = 1, // "never" 
	// Trigger layer, never collides with anything, only triggers/interacts.  Use when querying for interaction layers.
	COLLISION_GROUP_TRIGGER           = 2, // "trigger"
	// Conditionally solid means that the collision response will be zero or as defined in the table when there are matching interactions
	COLLISION_GROUP_CONDITIONALLY_SOLID = 3, // needs interactions
	// First unreserved collision layer index.
	COLLISION_GROUP_FIRST_USER        = 4,

	// Hard limit of 254 due to memory layout, and these are never visible to scene queries.
	COLLISION_GROUPS_MAX_ALLOWED      = 64,
};

enum BuiltInInteractionLayer_t
{
	// Slow path for generating interactions when INTERSECTION_INTERACTION_LAYER_MAX is hit.
	// Set by default on every m_interactsAs, but not any m_interactsWith.
	INTERACTION_LAYER_ALWAYS           = 0, // "always"
	// Indicates that the query will have to recurse into a hitbox model.
	INTERACTION_LAYER_HITBOXES         = 1,
	// Might as well hard-code "trigger" as generic interaction layer.  Character Controllers add it by default.
	INTERACTION_LAYER_TRIGGER          = 2,
	// First unreserved interaction layer index.
	INTERACTION_LAYER_SKY = 3,
	INTERACTION_LAYER_FIRST_USER       = 4,
	INTERACTION_LAYER_NOT_FOUND        = -1,
	// Number of scene query interaction layers limited by storing these in a 64-bit uint mask
	INTERACTION_LAYERS_MAX_ALLOWED              = 64,
};

enum IntersectionPairFlags_t
{
	INTERSECTION_PAIR_RESOLVE_CONTACTS		= (1<<0), 
	INTERSECTION_PAIR_NOTIFY_TOUCH_FOUND	= (1<<2), 
	INTERSECTION_PAIR_NOTIFY_TOUCH_PERSISTS = (1<<3), 
	INTERSECTION_PAIR_NOTIFY_TOUCH_LOST		= (1<<4), 
	
	INTERSECTION_PAIR_TRIGGER = INTERSECTION_PAIR_NOTIFY_TOUCH_FOUND | INTERSECTION_PAIR_NOTIFY_TOUCH_LOST,
	INTERSECTION_PAIR_DEFAULT_COLLISION = INTERSECTION_PAIR_RESOLVE_CONTACTS,

	INTERSECTION_NOTIFICATION_ANY =	INTERSECTION_PAIR_NOTIFY_TOUCH_FOUND | INTERSECTION_PAIR_NOTIFY_TOUCH_PERSISTS | INTERSECTION_PAIR_NOTIFY_TOUCH_LOST
};

enum IntersectionQueryResult_t
{
	INTERSECTION_QUERY_RESULT_FLAGS_NONE  = 0,
	INTERSECTION_QUERY_RESULT_FLAGS_BLOCK = 1,

};


enum CollisionFunctionMask_t
{
	FCOLLISION_FUNC_ENABLE_SOLID_CONTACT			= 1<<0,
	FCOLLISION_FUNC_ENABLE_TRACE_QUERY				= 1<<1,
	FCOLLISION_FUNC_ENABLE_TOUCH_EVENT				= 1<<2,
	FCOLLISION_FUNC_ENABLE_SHOULD_COLLIDE_CALLBACK	= 1<<3,
	FCOLLISION_FUNC_IGNORE_FOR_HITBOX_TEST			= 1<<4,
};


enum RnMassPriorityEnum_t
{
	MASS_PRIORITY_DEFAULT = 0,
	MASS_PRIORITY_TRIGGER = -1 // triggers never have mass that affects other physics objects
};



// Volume queries: Let's you decide between broadphase proxy and shape overlaps
enum OverlapTest_t
{
	OVERLAP_TEST_PROXY,	// This will only test the shapes broadphase proxy and not the shape itself
	OVERLAP_TEST_SHAPE	// This will first find all overlapping proxies inside the broadphase and then test the actual shape for overlap
};


// these are on by default
#define FCOLLISION_FUNC_DEFAULT		(FCOLLISION_FUNC_ENABLE_SOLID_CONTACT | FCOLLISION_FUNC_ENABLE_TRACE_QUERY | FCOLLISION_FUNC_ENABLE_TOUCH_EVENT)

extern CUtlString IntersectionPairFlagsToString( uint16 nPairFlags );

// TODO:  Currently these flags are stored in a table as 16-bits per-pair.  Once we have
// TODO:  identified the desired permutations of flags, the per-pair data will be
// TODO:  palletized down to a 4-bit index into a lookup table. 



// ==================================================================================================

typedef int32  IntersectionLayerIndex;
typedef int32  CollisionGroupIndex;
typedef uint16 IntersectionLayerPairFlags;
typedef uint16 CollisionGroupPairFlags;

struct RnCollisionAttr_t
{
public:
	//uintp m_nOwner;

	// Which interaction layers do I represent? (e.g. I am a INTERACTION_LAYER_PLAYERCLIP volume)
	// NOTE: This is analogous to "contents" in source 1  (bit mask of CONTENTS_* or 1<<INTERACTION_LAYER_*)
	uint64 m_nInteractsAs;

	// Which interaction layers do I interact or collide with? (e.g. I collide with INTERACTION_LAYER_PASSBULLETS because I am not a bullet)
	// NOTE: This is analogous to the "solid mask" or "trace mask" in source 1 (bit mask of CONTENTS_* or 1<<INTERACTION_LAYER_*)
	uint64 m_nInteractsWith;

	// Which interaction layers do I _not_ interact or collide with?  If my exclusion layers match m_nInteractsAs on the other object then no interaction happens.
	uint64 m_nInteractsExclude;

	uint32 m_nEntityId;				// this is the ID of the game entity
	uint16 m_nHierarchyId;			// this is an ID for the hierarchy of game entities (used to disable collision among objects in a hierarchy)
	uint8 m_nCollisionGroup;		// one of the registered collision groups
	uint8 m_nCollisionFunctionMask; // set of CollisionFunctionMask_t bits

public:
	inline RnCollisionAttr_t()
	{
		m_nInteractsAs = 0; //1 << INTERACTION_LAYER_ALWAYS;
		m_nInteractsWith = 0;
		m_nInteractsExclude = 0;
		m_nCollisionGroup = COLLISION_GROUP_ALWAYS;
		m_nCollisionFunctionMask = FCOLLISION_FUNC_DEFAULT;
		m_nEntityId = 0;
		m_nHierarchyId = 0;
	}

	uint64 GetUsedLayerMask() const												{ return m_nInteractsAs | m_nInteractsWith | m_nInteractsExclude; }
	uint64 GetUsedGroupMask() const												{ return 1ull << m_nCollisionGroup; }
	uint8 GetCollisionFunctionMask() const										{ return m_nCollisionFunctionMask; }
	void SetCollisionFunctionMask( uint8 nCollisionFunctionMask )				{ m_nCollisionFunctionMask = nCollisionFunctionMask; }
	inline bool AddCollisionFunctionMask( uint8 nAddMask );
	inline bool RemoveCollisionFunctionMask( uint8 nRemoveMask );
	inline void SetCollisionGroup( int nGroup )									{ m_nCollisionGroup = nGroup; }

	bool IsSolidContactEnabled() const											{ return (GetCollisionFunctionMask() & FCOLLISION_FUNC_ENABLE_SOLID_CONTACT) != 0; }
	bool IsTraceAndQueryEnabled() const											{ return (GetCollisionFunctionMask() & FCOLLISION_FUNC_ENABLE_TRACE_QUERY) != 0; }
	bool IsTouchEventEnabled() const											{ return (GetCollisionFunctionMask() & FCOLLISION_FUNC_ENABLE_TOUCH_EVENT) != 0; }
	bool IsShouldCollideCallbackEnabled() const									{ return (GetCollisionFunctionMask() & FCOLLISION_FUNC_ENABLE_SHOULD_COLLIDE_CALLBACK) != 0; }
	bool ShouldIgnoreForHitboxTest() const										{ return (GetCollisionFunctionMask() & FCOLLISION_FUNC_IGNORE_FOR_HITBOX_TEST) != 0; }

	inline bool HasInteractsAsLayer( int nLayerIndex ) const					{ return ( m_nInteractsAs & ( 1ull << nLayerIndex ) ) != 0; }
	inline bool HasInteractsWithLayer( int nLayerIndex ) const					{ return ( m_nInteractsWith & ( 1ull << nLayerIndex ) ) != 0; }
	inline bool HasInteractsExcludeLayer( int nLayerIndex ) const				{ return ( m_nInteractsExclude & ( 1ull << nLayerIndex ) ) != 0; }
	inline void EnableInteractsAsLayer( int nLayer )							{ m_nInteractsAs |= ( 1ull << nLayer ); }
	inline void EnableInteractsWithLayer( int nLayer )							{ m_nInteractsWith |= ( 1ull << nLayer ); }
	inline void EnableInteractsExcludeLayer( int nLayer )						{ m_nInteractsExclude |= ( 1ull << nLayer ); }
	inline void DisableInteractsAsLayer( int nLayer )							{ m_nInteractsAs &= ~( 1ull << nLayer ); }
	inline void DisableInteractsWithLayer( int nLayer )							{ m_nInteractsWith &= ~( 1ull << nLayer ); }
	inline void DisableInteractsExcludeLayer( int nLayer )						{ m_nInteractsExclude &= ~( 1ull << nLayer ); }

	inline uint32 GetEntityId() const											{ return m_nEntityId; }
	inline uint32 GetHierarchyId() const										{ return m_nHierarchyId; }
};


inline bool RnCollisionAttr_t::AddCollisionFunctionMask( uint8 nAddMask )
{
	if ( (m_nCollisionFunctionMask & nAddMask) != nAddMask )
	{
		m_nCollisionFunctionMask |= nAddMask;
		return true;
	}
	return false;
}

inline bool RnCollisionAttr_t::RemoveCollisionFunctionMask( uint8 nRemoveMask )
{
	if ( (m_nCollisionFunctionMask & nRemoveMask) != 0 )
	{
		m_nCollisionFunctionMask &= ~nRemoveMask;
		return true;
	}
	return false;
}

#endif
