//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// cs_simple_hostage.h
// Simple CS1.6 level hostage
// Author: Michael S. Booth, July 2004

#ifndef _CS_SIMPLE_HOSTAGE_H_
#define _CS_SIMPLE_HOSTAGE_H_

#include "nav_mesh.h"
#include "cs_nav_path.h"
#include "cs_nav_pathfind.h"
#include "improv_locomotor.h"
#include "cs_playeranimstate.h"
#include "ai_speech.h"

class CCSPlayer;

class CHostageExpresserShim : public CBaseCombatCharacter
{
public:
	inline CAI_Expresser *GetExpresser( void ) { return m_pExpresser; }
	inline const CAI_Expresser *GetMultiplayerExpresser( void ) const { return m_pExpresser; }

protected:
	CAI_Expresser *m_pExpresser;
};

class CHostageCarriableProp : public CBaseAnimating
{
public:
	DECLARE_CLASS( CHostageCarriableProp, CBaseAnimating );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	DECLARE_PREDICTABLE();
};

//----------------------------------------------------------------------------------------------------------------
/**
 * A Counter-Strike Hostage
 */
class CHostage : public CAI_ExpresserHost< CHostageExpresserShim >, public CImprovLocomotor, public ICSPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( CHostage, CHostageExpresserShim );
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CHostage( void );
	virtual ~CHostage();


public:
	virtual void Spawn( void );
	virtual void Precache();
	int ObjectCaps( void ) { return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE); }	// make hostage "useable"
	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual void PhysicsSimulate( void );

	virtual int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );

	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void Touch( CBaseEntity *other );				// in contact with "other"
	
	void HostageRescueZoneTouch( inputdata_t &inputdata );	// invoked when hostage touches a rescue zone

	void HostageThink( void );								// periodic update to initiate behaviors

	void GiveCTUseBonus( CCSPlayer *rescuer );				// give bonus to CT's for talking to a hostage
	void CheckForHostageAbuse( CCSPlayer *player );			// check for hostage-killer abuse

	// queries
	bool IsBeingCarried( void );
	bool IsFollowingSomeone( void );
	bool IsFollowing( const CBaseEntity *entity );
	bool IsValid( void ) const;
	bool IsRescuable( void ) const;
	bool IsRescued( void ) const;
	bool IsOnGround( void ) const;

	bool IsVisible( const Vector &pos, bool testFOV = false ) const;	// return true if hostage can see position

	// hostage states
	void Idle( void );										// stand idle
	void Follow( CCSPlayer *leader );						// begin following "leader"
	CCSPlayer *GetLeader( void ) const;						// return our leader, or NULL

	void DropHostage( Vector vecPosition, bool bIsRescued = false );

	void FaceTowards( const Vector &target, float deltaT );	// rotate body to face towards "target"
	void ApplyForce( const Vector &force )		{ m_accel += force; }	// apply a force to the hostage

	unsigned int PhysicsSolidMaskForEntity() const;
	Class_T Classify ( void ) { return CLASS_PLAYER_ALLY; }

public:
	// begin CImprovLocomotor -----------------------------------------------------------------------------------------------------------------
	virtual const Vector &GetCentroid( void ) const;
	virtual const Vector &GetFeet( void ) const;			// return position of "feet" - point below centroid of improv at feet level
	virtual const Vector &GetEyes( void ) const;
	virtual float GetMoveAngle( void ) const;				// return direction of movement

	virtual CNavArea *GetLastKnownArea( void ) const;
	virtual bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = NULL );	// find "simple" ground height, treating current nav area as part of the floor

	virtual void Crouch( void );
	virtual void StandUp( void );							// "un-crouch"
	virtual bool IsCrouching( void ) const;

	virtual void Jump( void );								// initiate a jump
	virtual bool IsJumping( void ) const;

	virtual void Run( void );								// set movement speed to running
	virtual void Walk( void );								// set movement speed to walking
	virtual bool IsRunning( void ) const;

	virtual void StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos );	// invoked when a ladder is encountered while following a path
	virtual bool TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT );	// traverse given ladder
	virtual bool IsUsingLadder( void ) const;

	virtual void TrackPath( const Vector &pathGoal, float deltaT );		// move along path by following "pathGoal"
	virtual void OnMoveToSuccess( const Vector &goal );					// invoked when an improv reaches its MoveTo goal
	virtual void OnMoveToFailure( const Vector &goal, MoveToFailureType reason );	// invoked when an improv fails to reach a MoveTo goal
	// end CImprovLocomotor -------------------------------------------------------------------------------------------------------------------

public:
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon();
	virtual bool CSAnim_CanMove();

	virtual void ModifyOrAppendCriteria( AI_CriteriaSet& set );

	int GetHostageState( void ) { return m_nHostageState; }

	uint32 GetHostageSpawnExclusionGroup() const { return m_uiHostageSpawnExclusionGroupMask; }
	uint32 GetHostageSpawnRandomFactor() const { return m_nHostageSpawnRandomFactor; }

	COutputEvent m_OnHostageBeginGrab;
	COutputEvent m_OnFirstPickedUp; 
	COutputEvent m_OnDroppedNotRescued; 
	COutputEvent m_OnRescued; 

protected:
	virtual CAI_Expresser *CreateExpresser( void );
	virtual void HostageUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SetHostageStartFollowingPlayer( CCSPlayer *pPlayer );
	void SmoothlyDropHostageToGround( Vector vecPosition );

private:
	uint32 m_uiHostageSpawnExclusionGroupMask;	// Mask of hostage spawn exclusion groups
	uint32 m_nHostageSpawnRandomFactor; // Additional weight when building random spawn locations
	bool m_bRemove; // We've exceeded MAX_HOSTAGES; delete excess entities upon spawn.

	float GetModifiedDamage( float flDamage, int nHitGroup );

	IPlayerAnimState *m_PlayerAnimState;

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iMaxHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_lifeState );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_fFlags );
	
	CNetworkVar( Vector, m_vel );

	CNetworkVar( bool, m_isRescued );						// true if the hostage has been rescued
	CNetworkVar( bool, m_jumpedThisFrame );

	CNetworkVar( int, m_nHostageState );

	CNetworkVar( EHANDLE, m_leader );						// the player we are following
	void UpdateFollowing( float deltaT );					// do following behavior

	int m_lastLeaderID;

	CountdownTimer m_reuseTimer;							// to throttle how often hostage can be used
	bool m_hasBeenUsed;										// flag to give first rescuer bonus money

	Vector m_accel;

	bool m_isRunning;										// true if hostage move speed is to run (walk if false)
	bool m_isCrouching;										// true if hostage is crouching
	CountdownTimer m_jumpTimer;								// if zero, we can jump

	bool m_isWaitingForLeader;								// true if we are waiting for our rescuer to move

	CCSNavPath m_path;										// current path to follow
	CountdownTimer m_repathTimer;							// throttle pathfinder

	CountdownTimer m_inhibitDoorTimer;

	CNavPathFollower m_pathFollower;						// path tracking mechanism
	CountdownTimer m_inhibitObstacleAvoidanceTimer;			// when active, turn off path following feelers

	CNavArea *m_lastKnownArea;								// last area we were in

	void Wiggle( void );									// attempt to wiggle-out of begin stuck
	CountdownTimer m_wiggleTimer;							// for wiggling
	NavRelativeDirType m_wiggleDirection;

	bool m_isAdjusted;										// hack for adjusting bounding box

	// Info for defusing.
	bool			m_bHandsHaveBeenCut;
	//bool			m_bBeingGrabbed;
	CHandle<CCSPlayer> m_pHostageGrabber;
	float			m_fLastGrabTime;
	float			m_flGrabbingLength;		//How long does the defuse take? Depends on if a defuser was used
	Vector			m_vecPositionWhenStartedDroppingToGround; // Where was the hostage when he started dropping to ground
	
	Vector			m_vecGrabbedPos;
	CNetworkVar( float, m_flRescueStartTime );
	CNetworkVar( float, m_flGrabSuccessTime );		//What time did the grabbing succeed?
	CNetworkVar( float, m_flDropStartTime );		//What time did the grabbing succeed?

	void PushawayThink( void );								// pushes physics objects away from the hostage
	void AvoidPhysicsProps( void );							// guides the hostage away from physics props
};


//--------------------------------------------------------------------------------------------------------------
/**
 * Functor used with NavAreaBuildPath() for building Hostage paths.
 * Once we hook up crouching and ladders, this can be removed and ShortestPathCost() can be used instead.
 */
class HostagePathCost
{
public:
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length )
	{
		if (fromArea == NULL)
		{
			// first area in path, no cost
			return 0.0f;
		}
		else
		{
			// if this area isn't available to hostages, skip it
			if ( area->GetAttributes() & NAV_MESH_NO_HOSTAGES )
			{
				return -1.0f;
			}

			// compute distance travelled along path so far
			float dist;

			if (ladder)
			{
				// can't traverse ladders
				return -1.0f;
			}
			else
			{
				dist = (area->GetCenter() - fromArea->GetCenter()).Length();
			}

			float cost = dist + fromArea->GetCostSoFar();

			if (area->GetAttributes() & NAV_MESH_CROUCH) //  && !(area->GetAttributes() & NAV_MESH_JUMP))
			{
				// can't traverse areas that require crouching
				return -1.0f;
			}

			// if this is a "jump" area, add penalty
			if (area->GetAttributes() & NAV_MESH_JUMP)
			{
				const float jumpPenalty = 5.0f;
				cost += jumpPenalty * dist;
			}

			return cost;
		}
	}
};


//--------------------------------------------------------------------------------------------------------------
// All the hostage entities.
extern CUtlVector< CHostage * > g_Hostages;


//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all active hostages in the game, invoking functor on each.
 * If functor returns false, stop iteration and return false.
 */
template < typename Functor >
bool ForEachHostage( Functor &func )
{
	for( int i=0; i<g_Hostages.Count(); ++i )
	{
		CHostage *hostage = g_Hostages[i];

		if ( hostage == NULL || !hostage->IsValid() )
			continue;

		if ( func( hostage ) == false )
			return false;
	}

	return true;
}


#endif // _CS_SIMPLE_HOSTAGE_H_
