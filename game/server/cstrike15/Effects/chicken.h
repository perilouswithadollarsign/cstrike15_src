// chicken.h
// An interactive, shootable chicken

#ifndef CHICKEN_H
#define CHICKEN_H

#include "props.h"
#include "GameEventListener.h"
#include "nav_mesh.h"
#include "cs_nav_path.h"
#include "cs_nav_pathfind.h"
#include "improv_locomotor.h"

class CCSPlayer;

class CChicken : public CDynamicProp, public CGameEventListener, public CImprovLocomotor
{
public:
	DECLARE_CLASS( CChicken, CDynamicProp );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CChicken();
	virtual ~CChicken();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void ChickenTouch( CBaseEntity *pOther );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	virtual void FireGameEvent( IGameEvent *event );

	virtual bool IsAlive( void ) { return true; }

	void ChickenThink( void );

	bool IsZombie( void ) { return m_flWhenZombified > 0; }
	void Zombify( void ) { m_flWhenZombified = gpGlobals->curtime; }

	bool IsFollowingSomeone( void );
	bool IsFollowing( const CBaseEntity *entity );
	bool IsOnGround( void ) const;

	void Follow( CCSPlayer *leader );						// begin following "leader"
	CCSPlayer *GetLeader( void ) const;						// return our leader, or NULL

	void FaceTowards( const Vector &target, float deltaT );	// rotate body to face towards "target"

	int	ObjectCaps( void )	{ return ( BaseClass::ObjectCaps( ) | FCAP_IMPULSE_USE ); }

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

	bool		CanJump( void ) const;
	void		Jump( float flVelocity );

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

protected:
	virtual void ChickenUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SetChickenStartFollowingPlayer( CCSPlayer *pPlayer );

private:
	void Idle();
//	void Walk();
	void Flee( CBaseEntity *fleeFrom, float duration );
	void Fly();
	void Land();

	void Update( void );
	CountdownTimer m_updateTimer;

	float AvoidObstacles( void );
	Vector m_stuckAnchor;
	CountdownTimer m_stuckTimer;

	void ResolveCollisions( const Vector &desiredPosition, float deltaT );
	bool m_isOnGround;

	Activity m_activity;
	CountdownTimer m_activityTimer;
	float m_turnRate;
	CHandle< CBaseEntity > m_fleeFrom;
	CountdownTimer m_moveRateThrottleTimer;

	CountdownTimer m_startleTimer;

	CountdownTimer m_vocalizeTimer;

	float m_flWhenZombified;

	CNetworkVar( bool, m_jumpedThisFrame );
	CNetworkVar( EHANDLE, m_leader );						// the player we are following


	void UpdateFollowing( float deltaT );					// do following behavior
	int m_lastLeaderID;

	CountdownTimer m_reuseTimer;							// to throttle how often hostage can be used
	bool m_hasBeenUsed;

	CountdownTimer m_jumpTimer;								// if zero, we can jump
	float m_flLastJumpTime;
	bool m_bInJump;

	bool m_isWaitingForLeader;								// true if we are waiting for our rescuer to move

	CCSNavPath m_path;										// current path to follow
	CountdownTimer m_repathTimer;							// throttle pathfinder

	CountdownTimer m_inhibitDoorTimer;

	CNavPathFollower m_pathFollower;						// path tracking mechanism
	CountdownTimer m_inhibitObstacleAvoidanceTimer;			// when active, turn off path following feelers

	CNavArea *m_lastKnownArea;								// last area we were in
	Vector m_vecPathGoal;

	float m_flActiveFollowStartTime;						// when the current follow started
};


#endif // CHICKEN_H
