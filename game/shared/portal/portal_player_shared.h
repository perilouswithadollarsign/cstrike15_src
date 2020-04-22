//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef PORTAL_PLAYER_SHARED_H
#define PORTAL_PLAYER_SHARED_H
#pragma once

#include "studio.h"
#include "paint_color_manager.h"
#include "cegclientwrapper.h"

#define PORTAL_PUSHAWAY_THINK_INTERVAL		(1.0f / 20.0f)

// Max mass the player can lift with +use
#define PORTAL_PLAYER_MAX_LIFT_MASS 85
#define PORTAL_PLAYER_MAX_LIFT_SIZE 128

#define PLAYERPORTALDEBUGSPEW 0

const char *GetEggBotModel( bool bLowRes = false );
const char *GetBallBotModel( bool bLowRes = false );

class ISignifierTarget
{
public:
	virtual bool	OverrideSignifierPosition( void ) = 0;
	virtual bool	GetSignifierPosition( const Vector &vSource, Vector &vPositionOut, Vector &vNormalOut ) = 0;
	virtual bool	GetSignifierDesignation( char *lpszBuffer, unsigned int nBufferSize ) = 0;
	virtual bool	UseSelectionGlow( void ) = 0;
};

class CSignifierTarget : public ISignifierTarget
{
public:
	virtual bool	UseSelectionGlow( void ) { return true; }
	virtual bool	OverrideSignifierPosition( void ) { return false; }
	virtual bool	GetSignifierPosition( const Vector &vSource, Vector &vPositionOut, Vector &vNormalOut ) { return false; }
	virtual bool	GetSignifierDesignation( char *lpszBuffer, unsigned int nBufferSize )
	{
		V_memset( lpszBuffer, 0, nBufferSize );
		return false;
	}
};

enum
{
	PLAYER_SOUNDS_CITIZEN = 0,
	PLAYER_SOUNDS_COMBINESOLDIER,
	PLAYER_SOUNDS_METROPOLICE,
	PLAYER_SOUNDS_MAX,
};

enum 
{
	CONCEPT_CHELL_IDLE,
	CONCEPT_CHELL_DEAD,
};

const float PLAYER_HULL_REDUCTION = 0.70f;

extern const char *g_pszChellConcepts[];
int GetChellConceptIndexFromString( const char *pszConcept );
extern ConVar sv_portal_coop_ping_hud_indicitator_duration;

struct PaintPowerInfo_t;

const float STEEP_SLOPE = 0.7;
char const* const PORTAL_PREDICTED_CONTEXT = "Portal Predicted Powers";

enum JumpButtonPress
{
	JUMP_ON_TOUCH = 0,
	PRESS_JUMP_TO_BOUNCE,
	HOLD_JUMP_TO_BOUNCE,
	TRAMPOLINE_BOUNCE
};

enum InAirState
{
	ON_GROUND,
	IN_AIR_JUMPED,
	IN_AIR_BOUNCED,
	IN_AIR_FELL
};

enum PaintSurfaceType
{
	FLOOR_SURFACE = 0,
	WALL_SURFACE,
	CEILING_SURFACE
};

enum StickCameraState
{
	STICK_CAMERA_SURFACE_TRANSITION = 0,
	STICK_CAMERA_ROLL_CORRECT,
	STICK_CAMERA_PORTAL,
	STICK_CAMERA_WALL_STICK_DEACTIVATE_TRANSITION,
	STICK_CAMERA_SWITCH_TO_ABS_UP_MODE,
	STICK_CAMERA_ABS_UP_MODE,
	STICK_CAMERA_SWITCH_TO_LOCAL_UP,
	STICK_CAMERA_SWITCH_TO_LOCAL_UP_LOOKING_UP,
	STICK_CAMERA_LOCAL_UP_LOOKING_UP,
	STICK_CAMERA_UPRIGHT
};

enum StickCameraCorrectionMethod
{
	QUATERNION_CORRECT = 0,
	ROTATE_UP,
	SNAP_UP,
	DO_NOTHING
};

//=============================================================================
// Paint Power Helper Functions
//=============================================================================
const Vector ComputeBouncePostVelocityNoReflect( const Vector& preVelocity,
												const Vector& normal,
												const Vector& up );

const Vector ComputeBouncePostVelocityReflection( const Vector& preVelocity,
												 const Vector& normal,
												 const Vector& localUp );

void ExpandAABB( Vector& boxMin, Vector& boxMax, const Vector& sweepVector );

//=============================================================================
// Paint Power Choice
//=============================================================================
struct PaintPowerChoiceCriteria_t
{
	Vector vNormInputDir;
	Vector vNormVelocity;
	bool bInPortal;
};

struct PaintPowerChoiceResult_t
{
	const PaintPowerInfo_t* pPaintPower;
	float flInputCos;
	float flVelocityCos;
	bool bWasIgnored;

	inline void Initialize()
	{
		pPaintPower = NULL;
		flInputCos = 1.0f;
		flVelocityCos = 2.0f;
		bWasIgnored = false;
	}
};

typedef CUtlVectorFixed< PaintPowerChoiceResult_t, PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER > PaintPowerChoiceResultArray;

struct CachedPaintPowerChoiceResult
{
	Vector surfaceNormal;
	CBaseHandle surfaceEntity;
	bool wasValid;
	bool wasIgnored;

	inline void Initialize()
	{
		surfaceNormal = Vector( 0, 0, 0 );
		surfaceEntity = NULL;
		wasValid = false;
		wasIgnored = false;
	}
};

//=============================================================================
// Contact Determination (used for determining available paint powers)
//=============================================================================
const int ALL_CONTENT = 0xFFFFFFFF;

struct BrushContact
{
	Vector point;
	Vector normal;
	CBaseEntity* pBrushEntity;
	bool isOnThinSurface;

	void Initialize( const Vector& contactPt,
					 const Vector& normal,
					 CBaseEntity* pBrushEntity,
					 bool onThinSurface );
	void Initialize( const fltx4& contactPt,
		const fltx4& normal,
		CBaseEntity* pBrushEntity,
		bool onThinSurface );
};

typedef CUtlVector<BrushContact> ContactVector;
typedef CUtlVector<cplane_t> CollisionPlaneVector;
void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask = CONTENTS_BRUSH_PAINT );
void ComputeAABBContactsWithBrushEntity( ContactVector& contacts, const cplane_t *pClipPlanes, int iClipPlaneCount, const Vector& boxOrigin, const Vector& boxMin, const Vector& boxMax, CBaseEntity* pBrushEntity, int contentsMask = CONTENTS_BRUSH_PAINT );

#ifndef CLIENT_DLL
class CPortal_Player;
#else
class C_Portal_Player;
#define CPortal_Player C_Portal_Player
#endif

void TracePlayerBoxAgainstCollidables( trace_t& trace,
									   const CPortal_Player* player,
									   const Vector& startPos,
									   const Vector& endPos,
									   const Vector& boxLocalMin,
									   const Vector& boxLocalMax );

struct StringCompare_t
{
	StringCompare_t( char const* str ) : m_str( str ) {}

	char const* const m_str;

	inline bool operator()( char const* str ) const
	{
		return V_strcmp( m_str, str ) == 0;
	}
};

#define PERMANENT_CONDITION		-1

// Player conditions for animations
enum
{
	PORTAL_COND_TAUNTING = 0,
	PORTAL_COND_POINTING,
	PORTAL_COND_DROWNING,
	PORTAL_COND_DEATH_CRUSH,
	PORTAL_COND_DEATH_GIB,
	PORTAL_COND_LAST
};


class CPortalPlayerShared
{
public:

	// Client specific.
#ifdef CLIENT_DLL

	friend class C_Portal_Player;
	typedef C_Portal_Player OuterClass;
	DECLARE_PREDICTABLE();

	// Server specific.
#else

	friend class CPortal_Player;
	typedef CPortal_Player OuterClass;

#endif

	DECLARE_EMBEDDED_NETWORKVAR()
	DECLARE_CLASS_NOBASE( CPortalPlayerShared );

	// Initialization.
	CPortalPlayerShared();
	void Init( OuterClass *pOuter );

	// Condition (PORTAL_COND_*)
	int		GetCond() const						{ return m_nPlayerCond; }
	void	SetCond( int nCond )				{ m_nPlayerCond = nCond; }
	void	AddCond( int nCond, float flDuration = PERMANENT_CONDITION );
	void	RemoveCond( int nCond );
	bool	InCond( int nCond );
	void	RemoveAllCond();
	void	OnConditionAdded( int nCond );
	void	OnConditionRemoved( int nCond );
	void	ConditionThink( void );
	float	GetConditionDuration( int nCond );

	void	ConditionGameRulesThink( void );
	void	DebugPrintConditions( void );

	bool	IsLoadoutUnavailable( void ) { return m_bLoadoutUnavailable; }
	void	SetLoadoutUnavailable( bool bUnavailable ) { m_bLoadoutUnavailable = bUnavailable; }

#ifdef CLIENT_DLL
	// This class only receives calls for these from C_TFPlayer, not
	// natively from the networking system
	virtual void OnPreDataChanged( void );
	virtual void OnDataChanged( void );

	// check the newly networked conditions for changes
	void	UpdateConditions( void );

#endif

private:
	// Vars that are networked.
	CNetworkVar( int, m_nPlayerCond );			// Player condition flags.
	CNetworkVar( bool, m_bLoadoutUnavailable );
	float m_flCondExpireTimeLeft[PORTAL_COND_LAST];		// Time until each condition expires

	// Vars that are not networked.
	OuterClass			*m_pOuter;					// C_TFPlayer or CTFPlayer (client/server).

	int	m_nOldConditions;

#ifdef GAME_DLL
	float	m_flNextCritUpdate;
	// FIXME: CUtlVector<CTFDamageEvent> m_DamageEvents;

	float	m_flTauntRemoveTime;

	// store damage info, so we can kill the player with this damage after crush animation is done
	CTakeDamageInfo m_damageInfo;
#endif
};


struct PortalPlayerStatistics_t
{
	DECLARE_CLASS_NOBASE( PortalPlayerStatistics_t );
	DECLARE_EMBEDDED_NETWORKVAR();

#ifdef GAME_DLL
	DECLARE_SIMPLE_DATADESC();
#endif

	CNetworkVar( int, iNumPortalsPlaced );
	CNetworkVar( int, iNumStepsTaken );
	CNetworkVar( float, fNumSecondsTaken );
	CNetworkVar( float, fDistanceTaken );
};


#if defined( CLIENT_DLL )
#define CPortal_Player C_Portal_Player
#define CPortalPlayerLocalData C_PortalPlayerLocalData
#endif


#endif //PORTAL_PLAYER_SHARED_h