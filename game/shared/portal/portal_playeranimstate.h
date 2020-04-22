//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_PLAYERANIMSTATE_H
#define PORTAL_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "multiplayer_animstate.h"


#if defined( CLIENT_DLL )
	class C_Portal_Player;
	#define CPortal_Player C_Portal_Player
#else
	class CPortal_Player;
#endif

//enum PlayerAnimEvent_t
//{
//	PLAYERANIMEVENT_FIRE_GUN=0,
//	PLAYERANIMEVENT_THROW_GRENADE,
//	PLAYERANIMEVENT_ROLL_GRENADE,
//	PLAYERANIMEVENT_JUMP,
//	PLAYERANIMEVENT_RELOAD,
//	PLAYERANIMEVENT_SECONDARY_ATTACK,
//
//	PLAYERANIMEVENT_HS_NONE,
//	PLAYERANIMEVENT_CANCEL_GESTURES,	// cancel current gesture
//
//	PLAYERANIMEVENT_COUNT
//};

enum PlayerAnimDamageStage_t
{
	DAMAGE_STAGE_NONE = 0,
	DAMAGE_STAGE_FINAL = 3
};

// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //
class CPortalPlayerAnimState : public CMultiPlayerAnimState
{
public:
	
	DECLARE_CLASS( CPortalPlayerAnimState, CMultiPlayerAnimState );

	CPortalPlayerAnimState();
	CPortalPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CPortalPlayerAnimState();

	void InitPortal( CPortal_Player *pPlayer );
	CPortal_Player *GetPortalPlayer( void ) const { return m_pPortalPlayer; }

	virtual void		ClearAnimationState();

	virtual Activity	TranslateActivity( Activity actDesired );
	virtual bool		SetupPoseParameters( CStudioHdr *pStudioHdr );
	virtual void		DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );
	virtual void		Update( float eyeYaw, float eyePitch );
	virtual Activity	CalcMainActivity();	

	void    Teleport( const Vector *pNewOrigin, const QAngle *pNewAngles, CPortal_Player* pPlayer );

	void				TransformYAWs( const matrix3x4_t &matTransform );

	virtual bool	ShouldLongFall( void ) const;

	virtual bool	HandleMoving( Activity &idealActivity );
	virtual bool	HandleJumping( Activity &idealActivity );
	virtual bool	HandleDucking( Activity &idealActivity );
	virtual bool	HandleDying( Activity &idealActivity );

	void BridgeRemovedFromUnder( void ) { m_bBridgeRemovedFromUnder = true; }

	float		m_fNextBouncePredictTime;
	float		m_fPrevBouncePredict;

private:
	bool HandleInAir( Activity &idealActivity );
	bool HandleBouncing( Activity &idealActivity );
	bool HandleTractorBeam( Activity &idealActivity );
	bool HandleLanding();

	void IncreaseDamageStage();
	
	CPortal_Player   *m_pPortalPlayer;
	bool		m_bInAirWalk;
	bool		m_bLanding;

	Vector		m_vLastVelocity;
	float		m_flHoldDeployedPoseUntilTime;
	unsigned int m_nDamageStage;

	// tractor beam
	bool		m_bWasInTractorBeam;
	bool		m_bFirstTractorBeamFrame;
	bool		m_bBridgeRemovedFromUnder;
};


CPortalPlayerAnimState* CreatePortalPlayerAnimState( CPortal_Player *pPlayer );


// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;


#endif // PORTAL_PLAYERANIMSTATE_H
