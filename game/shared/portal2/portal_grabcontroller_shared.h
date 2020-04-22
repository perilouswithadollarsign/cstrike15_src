//===== Copyright © Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//
//=============================================================================//

#ifndef PORTAL_GRABCONTROLLER_SHARED_H
#define PORTAL_GRABCONTROLLER_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#if defined ( CLIENT_DLL )
#include "c_baseanimating.h"
#include "player_pickup.h"
#endif

//#define DEBUG_SHADOW_CONTROLLER

class CPortal_Player;

// derive from this so we can add save/load data to it
struct game_shadowcontrol_params_t : public hlshadowcontrol_params_t
{
#if !defined ( CLIENT_DLL )
	DECLARE_SIMPLE_DATADESC();
#endif //!CLIENT_DLL

#if defined ( DEBUG_SHADOW_CONTROLLER ) 
	void SpewState( void )
	{
		Msg( "pos: %f %f %f rot: %f %f %f maxang %f maxDampAng %f maxSpd %f maxDampSpd %f dampFac %f tpDist %f\n", 
			XYZ( targetPosition ), XYZ( targetRotation ), maxAngular, maxDampAngular, maxSpeed, maxDampSpeed, dampFactor, teleportDistance );	
	}
#endif
};

//-----------------------------------------------------------------------------
class CGrabController : public IMotionEvent
{
#if !defined ( CLIENT_DLL )
	DECLARE_SIMPLE_DATADESC();
#endif //!CLIENT_DLL
	DECLARE_CLASS_NOBASE( CGrabController );

public:

	CGrabController( void );
	~CGrabController( void );

	virtual void AttachEntity( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition );
	virtual void AttachEntityVM( CBasePlayer *pPlayer, CBaseEntity *pEntity, IPhysicsObject *pPhys, bool bIsMegaPhysCannon, const Vector &vGrabPosition, bool bUseGrabPosition );

	virtual bool DetachEntity( bool bClearVelocity );
	virtual bool DetachEntityVM( bool bClearVelocity );
#if defined( CLIENT_DLL )
	virtual void DetachUnknownEntity( void ); //detach from current entity in a way that doesn't touch its pointed memory because we're not sure if it's valid anymore
#endif
	
#if !defined ( CLIENT_DLL )
	void OnRestore();
	float GetSavedMass( IPhysicsObject *pObject );
#endif //!CLIENT_DLL

	float GetObjectOffset( CBaseEntity *pEntity ) const;
	float GetObjectDistance( void ) const;

	virtual bool UpdateObject( CBasePlayer *pPlayer, float flError, bool bIsTeleport = false );
	virtual bool UpdateObjectVM( CBasePlayer *pPlayer, float flError );

	void SetTargetPosition( const Vector &target, const QAngle &targetOrientation, bool bIsTeleport = false );
	void GetTargetPosition( Vector *target, QAngle *targetOrientation );
	float ComputeError();
	float GetLoadWeight( void ) const { return m_flLoadWeight; }
	void SetAngleAlignment( float alignAngleCosine ) { m_angleAlignment = alignAngleCosine; }
	void SetIgnorePitch( bool bIgnore ) { m_bIgnoreRelativePitch = bIgnore; }
	QAngle TransformAnglesToPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer );
	QAngle TransformAnglesFromPlayerSpace( const QAngle &anglesIn, CBasePlayer *pPlayer );
	Vector TransformVectorToPlayerSpace( const Vector &vectorIn, CBasePlayer *pPlayer );
	Vector TransformVectorFromPlayerSpace( const Vector &vectorIn, CBasePlayer *pPlayer );
	void RotateObject( CBasePlayer *pPlayer, float fRotAboutUp, float fRotAboutRight, bool bUseWorldUpInsteadOfPlayerUp );

	CBaseEntity *GetAttached( void ) { return m_attachedEntity; }

	IMotionEvent::simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	
#if defined( CLIENT_DLL )
	virtual void ClientApproachTarget( CBasePlayer *pOwnerPlayer ); //client-only version of Simulate() to use in prediction without any hope of having a physics object or valid physics environment
	const Vector &GetHeldObjectRenderOrigin( void );
#endif

	//set when a held entity is penetrating another through a portal. Needed for special fixes
	void SetPortalPenetratingEntity( CBaseEntity *pPenetrated );

	// Compute the max speed for an attached object
	void ComputeMaxSpeed( CBaseEntity *pEntity, IPhysicsObject *pPhysics );

	bool IsUsingVMGrab( CBasePlayer *pPlayer = NULL ) const;
	bool WantsVMGrab( CBasePlayer *pPlayer = NULL ) const;

#if defined( GAME_DLL )
	//The held object teleported. Do some checks here to make sure we don't oscillate between pushing the object into and out of the portal every tick (bugbait #79020).
	//Mostly happens when we unintentionally teleport while physics is trying to go toward an unreachable target position.
	void CheckPortalOscillation( CPortal_Base2D *pWentThroughPortal, CBaseEntity *pTeleportingEntity, CPortal_Player *pHoldingPlayer );
#endif

public:

	game_shadowcontrol_params_t	m_shadow;
#if !defined ( CLIENT_DLL )
	float			m_savedRotDamping[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	float			m_savedMass[VPHYSICS_MAX_OBJECT_LIST_COUNT];
#endif //!CLIENT_DLL

	float			m_timeToArrive;
	float			m_errorTime;
	float			m_error;
	float			m_contactAmount;
	float			m_angleAlignment;
	bool			m_bCarriedEntityBlocksLOS;
	bool			m_bIgnoreRelativePitch;

	float			m_flLoadWeight;
	bool			m_bWasDragEnabled;

	EHANDLE			m_attachedEntity;

	QAngle			m_vecPreferredCarryAngles;
	bool			m_bHasPreferredCarryAngles;
	float			m_flDistanceOffset;

	QAngle			m_attachedAnglesPlayerSpace;
	Vector			m_attachedPositionObjectSpace;

	IPhysicsMotionController *m_controller;

	bool			m_bAllowObjectOverhead; // Can the player hold this object directly overhead? (Default is NO)

	//set when a held entity is penetrating another through a portal. Needed for special fixes
	EHANDLE			m_PenetratedEntity;

	float			m_fPlayerSpeed; //the owning player's speed. Held between UpdateObject() and Simulate()

	EHANDLE			m_hHoldingPlayer;

#if !defined ( CLIENT_DLL )
	friend void GetSavedParamsForCarriedPhysObject( CGrabController *pGrabController, IPhysicsObject *pObject, float *pSavedMassOut, float *pSavedRotationalDampingOut );
#endif //!CLIENT_DLL

private:

	bool FindSafePlacementLocation( Vector *pVecPosition, bool bFinalPass = false );
	void PushNearbyTurrets( void );

	void ShowDenyPlacement( void );
	float m_flAngleOffset;
	float m_flLengthOffset;
	float m_flTimeOffset;

	// Grr... We're juggling 3 different types of 
	// pickup logic, and two of them require swapping collision groups.
	// so we need two temps. One for VM mode changing to interactive debris
	int m_preVMModeCollisionGroup;
	int m_prePickupCollisionGroup;
	int m_oldTransmitState;
	bool m_bOldShadowState;
	EHANDLE m_hOldLightingOrigin;

	bool m_bOldUsingVMGrabState;

#if defined( CLIENT_DLL )	
	CDiscontinuousInterpolatedVar<Vector> m_iv_predictedRenderOrigin; //chances are that our attached object will get a crapton of network errors, resetting it's origin interpolator frequently. Keep a separate history
	Vector m_vHeldObjectRenderOrigin;
	bool bLastUpdateWasOnOppositeSideOfPortal;
#endif
};

//-----------------------------------------------------------------------------
// Player pickup controller
//-----------------------------------------------------------------------------
class CPlayerPickupController : public CBaseEntity
{
#if !defined ( CLIENT_DLL )
	DECLARE_DATADESC();
#endif //!CLIENT_DLL
	DECLARE_CLASS( CPlayerPickupController, CBaseEntity );
public:
	virtual void InitGrabController( CBasePlayer *pPlayer, CBaseEntity *pObject );
	virtual bool Shutdown( bool bThrown = false );
	bool OnControls( CBaseEntity *pControls ) { return true; }
	bool UsePickupController( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
#if !defined ( CLIENT_DLL )
	virtual void OnRestore()
	{
		m_grabController.OnRestore();
	}
#endif //!CLIENT_DLL
	void VPhysicsUpdate( IPhysicsObject *pPhysics ) {}
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics ) {}

	bool IsHoldingEntity( CBaseEntity *pEnt );
	CGrabController &GetGrabController();

public:
	CGrabController m_grabController;
	CBasePlayer			*m_pPlayer;
};


#if defined ( CLIENT_DLL )
class C_PlayerHeldObjectClone : public C_BaseAnimating, public CDefaultPlayerPickupVPhysics
{
public:
	DECLARE_CLASS( C_PlayerHeldObjectClone, C_BaseAnimating );

	~C_PlayerHeldObjectClone();

	bool InitClone( C_BaseEntity *pObject, C_BasePlayer *pPlayer, bool bIsViewModel = true, C_PlayerHeldObjectClone *pVMToFollow = NULL );
	void ClientThink( void );
	
	virtual bool OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

#if 0
	virtual void GetColorModulation( float* color );
#endif

	//IPlayerPickupVPhysics
	virtual bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle PreferredCarryAngles( void );

	CHandle< C_BasePlayer > m_hPlayer;
	EHANDLE m_hOriginal;
	int m_nOldSkin;
	bool m_bOnOppositeSideOfPortal;

	C_PlayerHeldObjectClone *m_pVMToFollow;
	Vector m_vPlayerRelativeOrigin; //Interpolators causing too much grief, just store render origin relative to the player's eye origin/angles and reconstruct world position when asked
};
#endif


bool PlayerPickupControllerIsHoldingEntity( CBaseEntity *pPickupController, CBaseEntity *pHeldEntity );
void ShutdownPickupController( CBaseEntity *pPickupControllerEntity );
float PlayerPickupGetHeldObjectMass( CBaseEntity *pPickupControllerEntity, IPhysicsObject *pHeldObject );
float PhysCannonGetHeldObjectMass( CBaseCombatWeapon *pActiveWeapon, IPhysicsObject *pHeldObject );

CBaseEntity *PhysCannonGetHeldEntity( CBaseCombatWeapon *pActiveWeapon );
CBaseEntity *GetPlayerHeldEntity( CBasePlayer *pPlayer );
CBasePlayer *GetPlayerHoldingEntity( const CBaseEntity *pEntity );

CGrabController *GetGrabControllerForPlayer( CBasePlayer *pPlayer );
CGrabController *GetGrabControllerForPhysCannon( CBaseCombatWeapon *pActiveWeapon );
void GetSavedParamsForCarriedPhysObject( CGrabController *pGrabController, IPhysicsObject *pObject, float *pSavedMassOut, float *pSavedRotationalDampingOut );
void UpdateGrabControllerTargetPosition( CBasePlayer *pPlayer, Vector *vPosition, QAngle *qAngles, bool bIsTeleport = false );
bool PhysCannonAccountableForObject( CBaseCombatWeapon *pPhysCannon, CBaseEntity *pObject );

void GrabController_SetPortalPenetratingEntity( CGrabController *pController, CBaseEntity *pPenetrated );

void RotatePlayerHeldObject( CBasePlayer *pPlayer, float fRotAboutUp, float fRotAboutRight, bool bUseWorldUpInsteadOfPlayerUp );


#endif // PORTAL_GRABCONTROLLER_SHARED_H
