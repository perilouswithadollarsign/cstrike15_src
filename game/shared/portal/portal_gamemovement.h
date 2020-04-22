//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Special handling for Portal usable ladders
//
//=============================================================================//
#ifndef PORTAL_GAMEMOVEMENT_H
#define PORTAL_GAMEMOVEMENT_H

#include "cbase.h"
#include "hl_gamemovement.h"

#if defined( CLIENT_DLL )
	#include "c_portal_player.h"
#else
	#include "portal_player.h"
#endif

class CTrace_PlayerAABB_vs_Portals;

//-----------------------------------------------------------------------------
// Purpose: Portal specific movement code
//-----------------------------------------------------------------------------
class CPortalGameMovement : public CGameMovement
{
	typedef CGameMovement BaseClass;
public:

	CPortalGameMovement();

	bool	m_bInPortalEnv;
// Overrides
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual const Vector&	GetPlayerMins( bool ducked ) const;
	virtual const Vector&	GetPlayerMaxs( bool ducked ) const;
	virtual const Vector&	GetPlayerViewOffset( bool ducked ) const;
	virtual void SetupMovementBounds( CMoveData *pMove );
	virtual bool CheckJumpButton( void );

	Vector PortalFunnel( const Vector &wishdir );

	// Traces the player bbox as it is swept from start to end
	virtual void TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, CTrace_PlayerAABB_vs_Portals& pm );
	virtual void TracePlayerBBox( const Vector& start, const Vector& end, unsigned int fMask, int collisionGroup, trace_t& pm );

	// Tests the player position
	virtual CBaseHandle	TestPlayerPosition( const Vector& pos, int collisionGroup, trace_t& pm );

	virtual int CheckStuck( void );

	virtual void SetGroundEntity( trace_t *pm );

	void HandlePortalling( void );

protected:

	CPortal_Player	*GetPortalPlayer() const;
	bool IsInPortalFunnelVolume( const Vector& vPlayerToPortal, const CPortal_Base2D* pPortal, const float flExtentX, const float flExtentY ) const;

	// Does most of the player movement logic.
	// Returns with origin, angles, and velocity modified in place.
	// were contacted during the move.
	virtual void	PlayerMove();

	// Handles both ground friction and water friction
	virtual void	Friction();

	virtual void	TBeamMove();

	virtual void	AirMove();
	virtual void	AirAccelerate( Vector& wishdir, float wishspeed, float accel );

	// Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
	virtual void	WalkMove();

	virtual void	WaterMove();

	// Try to keep a walking player on the ground when running down slopes etc
	virtual void	StayOnGround();

	virtual void	CheckWallImpact( Vector& primal_velocity );

	// Handle MOVETYPE_WALK.
	virtual void	FullWalkMove();

	// Implement this if you want to know when the player collides during OnPlayerMove
	virtual void	OnTryPlayerMoveCollision( trace_t &tr ) {}

	virtual const Vector&	GetPlayerMins() const; // uses local player
	virtual const Vector&	GetPlayerMaxs() const; // uses local player

	// Decompoosed gravity
	virtual void	StartGravity();
	virtual void	FinishGravity();

	// Apply normal ( undecomposed ) gravity
	virtual void	AddGravity();

	// The basic solid body movement clip that slides along multiple planes
	virtual int		TryPlayerMove( Vector *pFirstDest = NULL, trace_t *pFirstTrace = NULL );

	// Slide off of the impacting object
	// returns the blocked flags:
	// 0x01 == floor
	// 0x02 == step / wall
	virtual int		ClipVelocity( Vector& in, Vector& normal, Vector& out, float overbounce );

	// Determine if player is in water, on ground, etc.
	virtual void	CategorizePosition();

	virtual void	CheckParameters();

	virtual void	PlayerRoughLandingEffects( float fvol );
	virtual void	PlayerWallImpactEffects( float fvol, float normalVelocity );
	virtual void	PlayerCeilingImpactEffects( float fvol );

	// Ducking
	virtual void	Duck();
	virtual void	FinishUnDuck();
	virtual void	FinishDuck();
	virtual bool	CanUnduck();
	virtual void	UpdateDuckJumpEyeOffset();
	virtual bool	CanUnDuckJump( trace_t &trace );
	virtual void	StartUnDuckJump();
	virtual void	FinishUnDuckJump( trace_t &trace );
	virtual void	SetDuckedEyeOffset( float duckFraction );
	virtual void	FixPlayerCrouchStuck( bool moveup );

	virtual void	CategorizeGroundSurface( trace_t &pm );

	virtual void	StepMove( Vector &vecDestination, trace_t &trace );

	virtual bool	GameHasLadders() const { return false; }

private:
	virtual bool PlayerShouldFunnel( const CPortal_Base2D* pPortal, const Vector& vPlayerForward, const Vector& wishDir ) const;

	void AirPortalFunnel( Vector& wishdir,
						  const Vector& vPlayerToFunnelPortal,
						  float flExtraFunnelForce,
						  float flTimeToPortal );

	void GroundPortalFunnel( Vector& wishdir,
							 const Vector& vPlayerToPortalFunnel,
							 const Vector& vPortalNormal,
							 float flExtraFunnelForce,
							 float flTimeToPortal );

#if defined( CLIENT_DLL )
	void ClientVerticalElevatorFixes( CBasePlayer *pPlayer, CMoveData *pMove );
#endif

	// Stick is done by changing gravity direction because it's easier to think about that way.
	// Gravity direction is always the negation of the player's stick normal.
	Vector m_vGravityDirection;	

	Vector m_vMoveStartPosition; //where the player started before the movement code ran
};


//trace that has special understanding of how to handle portals
class CTrace_PlayerAABB_vs_Portals : public CGameTrace
{
public:
	CTrace_PlayerAABB_vs_Portals( void );
	bool HitPortalRamp( const Vector &vUp );

	bool m_bContactedPortalTransitionRamp;
};


#endif //PORTAL_GAMEMOVEMENT_H