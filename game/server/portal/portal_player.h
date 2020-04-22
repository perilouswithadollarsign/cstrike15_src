//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef PORTAL_PLAYER_H
#define PORTAL_PLAYER_H
#pragma once

class CPortal_Player;

#include "player.h"
#include "portal_playeranimstate.h"
#include "portal_playerlocaldata.h"
#include "simtimer.h"
#include "soundenvelope.h"
#include "npc_security_camera.h"
#include "portal_player_shared.h"
#include "weapon_portalbase.h"
#include "in_buttons.h"
#include "ai_speech.h"			// For expresser host
#include "basemultiplayerplayer.h"
#include "paint_power_user.h"
#include "paintable_entity.h"
#include "ai_basenpc.h"
#include "npc_security_camera.h"
#include "portal_base2d.h"

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	#include "portal2_item_inventory.h"
#endif

extern bool UTIL_TimeScaleIsNonStandard( void );


#define PORTAL_COLOR_FLAG_BLUE		( 1 << 0 )
#define PORTAL_COLOR_FLAG_PURPLE	( 1 << 1 )
#define PORTAL_COLOR_FLAG_ORANGE	( 1 << 2 )
#define PORTAL_COLOR_FLAG_RED		( 1 << 3 )

enum PlayerGunType
{
	PLAYER_NO_GUN = 0,
	PLAYER_PAINT_GUN,
	PLAYER_PORTAL_GUN
};

enum ForcedGrabControllerType
{
	FORCE_GRAB_CONTROLLER_DEFAULT = 0,
	FORCE_GRAB_CONTROLLER_VM,
	FORCE_GRAB_CONTROLLER_PHYSICS
};

class CEntityPortalledNetworkMessage
{
public:
	DECLARE_CLASS_NOBASE( CEntityPortalledNetworkMessage );

	CEntityPortalledNetworkMessage( void );

	CHandle<CBaseEntity> m_hEntity;
	CHandle<CPortal_Base2D> m_hPortal;
	float m_fTime;
	bool m_bForcedDuck;
	uint32 m_iMessageCount;
};

class CMoveData;

//=============================================================================
// >> Portal_Player
//=============================================================================
class CPortal_Player : public PaintPowerUser< CPaintableEntity< CBaseMultiplayerPlayer > >
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	, public IInventoryUpdateListener
#endif
{
public:
	DECLARE_CLASS( CPortal_Player, PaintPowerUser< CPaintableEntity< CBaseMultiplayerPlayer > > );

	CPortal_Player();
	virtual ~CPortal_Player( void );
	
	static CPortal_Player *CreatePlayer( const char *className, edict_t *ed );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();

	virtual void Precache( void );
	virtual void CreateSounds( void );
	virtual void StopLoopingSounds( void );
	virtual void Spawn( void );
	virtual void SharedSpawn();
	virtual void OnRestore( void );
	virtual int	Restore( IRestore &restore );
	virtual void Activate( void );
	
	virtual void Touch( CBaseEntity *pOther );

#ifdef PORTAL2
	virtual void InitialSpawn( void );

	void	SetPlacingPhoto( bool bPlacing );
	void	OnPhotoAdded( int nIndex );
	void	OnPhotoRemoved( int nIndex );
	void	SetSelectedPhoto( int nIndex );
	int		GetSelectedPhoto( void );
	void	ClearPhotos( void );
	void	StripPhotos( bool bNotifyPlayer = true );
	void	FlashDenyIndicator( float flDuration, unsigned char nType );
	void	FlashInventory( float flDuration, unsigned char nType );
	void	Flash( float flDuration, const Vector &vecPosition );
	void	ControlHelperAnimate( unsigned char nActiveIcon, bool bClear = false );
	void	UpdateLocatorEntityIndices( int *pIndices, int nNumIndices );
	void	SetMotionBlurAmount( float flAmt ) { m_flMotionBlurAmount = flAmt; }
#endif // PORTAL2

	virtual bool	ShouldCollide( int collisionGroup, int contentsMask ) const;
	virtual float	PlayScene( const char *pszScene, float flDelay, AI_Response *response, IRecipientFilter *filter );
	virtual void	NotifySystemEvent( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

	virtual void PostThink( void );
	virtual void PreThink( void );
	void SwapThink();
	virtual void PlayerDeathThink();
	virtual void PlayerTransitionCompleteThink();
	virtual void PlayerCatchPatnerNotConnectingThink();

	void UpdatePortalPlaneSounds( void );
	void UpdateWooshSounds( void );

	Activity TranslateActivity( Activity ActToTranslate, bool *pRequired = NULL );
	virtual void Teleport( const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity, bool bUseSlowHighAccuracyContacts = true );

	Activity TranslateTeamActivity( Activity ActToTranslate );

	virtual void SetAnimation( PLAYER_ANIM playerAnim );

	virtual void PlayerRunCommand(CUserCmd *ucmd, IMoveHelper *moveHelper);

	virtual bool ClientCommand( const CCommand &args );
	virtual void CreateViewModel( int viewmodelindex = 0 );
	virtual bool BecomeRagdollOnClient( const Vector &force );
	virtual int	OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual int	OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info );
	virtual bool WantsLagCompensationOnEntity( const CBasePlayer *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;
	virtual void FireBullets ( const FireBulletsInfo_t &info );
	
	virtual bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0);
	virtual Vector Weapon_ShootPosition();
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual void Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual void SelectItem( const char *pstr, int iSubType );
	
	
	virtual void ShutdownUseEntity( void );
	virtual bool ShouldDropActiveWeaponWhenKilled( void ) { return false; }

	virtual Vector	EyeDirection3D( void );
	virtual Vector	EyeDirection2D( void );
	virtual const Vector	&WorldSpaceCenter() const;

	//virtual bool StartReplayMode( float fDelay, float fDuration, int iEntity  );
	//virtual void StopReplayMode();
 	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void Jump( void );
	
	void UnDuck( void );
	bool UseFoundEntity( CBaseEntity *pUseEntity, bool bAutoGrab );
	bool IsInvalidHandoff( CBaseEntity *pObject );
	void PollForUseEntity( bool bBasicUse, CBaseEntity **ppUseEnt, CPortal_Base2D **ppUseThroughPortal );
	CBaseEntity* FindUseEntity( CPortal_Base2D **pThroughPortal );
	CBaseEntity* FindUseEntityThroughPortal( void );

	void ZoomIn( void );
	void ZoomOut( void );
	virtual bool IsZoomed( void );

	virtual void PlayerUse( void );
	//virtual bool StartObserverMode( int mode );
	virtual void GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void UpdateOnRemove( void );

	virtual void OnSave( IEntitySaveUtils *pUtils ); 

	virtual void SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );
	virtual void UpdatePortalViewAreaBits( unsigned char *pvs, int pvssize );
	virtual void ItemPostFrame( void );

	bool	ValidatePlayerModel( const char *pModel );

	void ClearScriptedInteractions( void );
	void ParseScriptedInteractions( void );
	void AddScriptedInteraction( ScriptedNPCInteraction_t *pInteraction );
	CUtlVector<ScriptedNPCInteraction_t> *GetScriptedInteractions( void ) { return &m_ScriptedInteractions; }

	void	FireConcept( const char *pConcept );
	bool	IsTaunting( void );
	int		GetTeamTauntState( void ) const { return m_nTeamTauntState; }
	void	SetTeamTauntState( int nTeamTauntState );
	Vector	GetTeamTauntPosition( void ) { return m_vTauntPosition; }
	QAngle	GetTeamTauntAngles( void ) { return m_vTauntAngles; }
	QAngle	GetTrickFireAngles( void ) { return m_vPreTauntAngles; }
	bool IsTrickFiring( void ) const { return m_bTrickFire; }
	void ClearTrickFiring( void ) { m_bTrickFire = false; }

	QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles.Get(); }

	Vector GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );

	virtual void CheatImpulseCommands( int iImpulse );
	void CreateRagdollEntity( const CTakeDamageInfo &info );
	void GiveAllItems( void );

	void GiveDefaultItems( void );

	void NoteWeaponFired( void );

	void ResetAnimation( void );

	void SetPlayerModel( void );
	
	int	  GetPlayerModelType( void ) { return m_iPlayerSoundType; }

	virtual void ForceDuckThisFrame( void );
	//void UnDuck ( void );
	//inline void ForceJumpThisFrame( void ) { ForceButtons( IN_JUMP ); }

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	void SetupBones( matrix3x4a_t *pBoneToWorld, int boneMask );

	// physics interactions
	virtual void PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize );
	virtual void ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis );

	void ToggleHeldObjectOnOppositeSideOfPortal( void ) { m_bHeldObjectOnOppositeSideOfPortal = !m_bHeldObjectOnOppositeSideOfPortal; }
	void SetHeldObjectOnOppositeSideOfPortal( bool p_bHeldObjectOnOppositeSideOfPortal ) { m_bHeldObjectOnOppositeSideOfPortal = p_bHeldObjectOnOppositeSideOfPortal; }
	bool IsHeldObjectOnOppositeSideOfPortal( void ) { return m_bHeldObjectOnOppositeSideOfPortal; }
	CPortal_Base2D *GetHeldObjectPortal( void ) const { return m_hHeldObjectPortal.Get(); }
	void SetHeldObjectPortal( CPortal_Base2D *pPortal ) { m_hHeldObjectPortal = pPortal; }
	void SetUsingVMGrabState( bool bState ) { m_bUsingVMGrabState = bState; }
	bool IsUsingVMGrab( void );
	bool WantsVMGrab( void );
	void UpdateVMGrab( CBaseEntity *pEntity );
	bool IsForcingDrop( void ) { return m_bForcingDrop; }

	// This is set by the client when it picks something up
	// and used to initiate server grab logic. If we actually pick something up
	// it's held in 'm_hAttachedObject'. 
	EHANDLE m_hGrabbedEntity;
	EHANDLE m_hPortalThroughWhichGrabOccured;
	bool m_bForcingDrop;
	CNetworkVar( bool, m_bUseVMGrab );
	CNetworkVar( bool, m_bUsingVMGrabState );
	float m_flUseKeyStartTime;	// for long duration uses, record the initial keypress start time
	float m_flAutoGrabLockOutTime;

	void SetForcedGrabControllerType( ForcedGrabControllerType type );
	ForcedGrabControllerType m_ForcedGrabController;

	// Object we're successfully holding we network down to the client
	// for clientside simulation under multiplayer
	CNetworkHandle( CBaseEntity, m_hAttachedObject );
	CNetworkQAngle( m_vecCarriedObjectAngles );
	//not simulating physics on the client, network down any inability the held object has in reaching it's target position/orientation
	CNetworkVector( m_vecCarriedObject_CurPosToTargetPos );
	CNetworkQAngle( m_vecCarriedObject_CurAngToTargetAng );

	void SetUseKeyCooldownTime( float flCooldownDuration );

	void SetStuckOnPortalCollisionObject( void ) { m_bStuckOnPortalCollisionObject = true; }

	CWeaponPortalBase* GetActivePortalWeapon() const;

	void IncrementPortalsPlaced( bool bSecondaryPortal );
	void IncrementStepsTaken( void );
	void IncrementDistanceTaken( void );
	void UpdateSecondsTaken( void );
	void ResetThisLevelStats( void );
	int NumPortalsPlaced( void ) const { return m_StatsThisLevel.iNumPortalsPlaced; }
	int NumStepsTaken( void ) const { return m_StatsThisLevel.iNumStepsTaken; }
	float NumSecondsTaken( void ) const { return m_StatsThisLevel.fNumSecondsTaken; }
	float NumDistanceTaken( void ) const { return m_StatsThisLevel.fDistanceTaken; }
	
	bool	IsHoldingEntity( CBaseEntity *pEnt );
	float	GetHeldObjectMass( IPhysicsObject *pHeldObject );

	void SetNeuroToxinDamageTime( float fCountdownSeconds ) { m_fNeuroToxinDamageTime = gpGlobals->curtime + fCountdownSeconds; }

	void IncNumCamerasDetatched( void ) { ++m_iNumCamerasDetatched; }
	int GetNumCamerasDetatched( void ) const { return m_iNumCamerasDetatched; }

	void MarkClientCheckPVSDirty( void ) { m_bClientCheckPVSDirty = true; }

	void SetIsHoldingObject( bool bSet ) { m_bIsHoldingSomething = bSet; }

	virtual void ApplyPortalTeleportation( const CPortal_Base2D *pEnteredPortal, CMoveData *pMove ); 

	Vector m_vecTotalBulletForce;	//Accumulator for bullet force in a single frame

	bool m_bSilentDropAndPickup;

	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle

#if USE_SLOWTIME
	bool IsSlowingTime( void ) { return ( m_PortalLocal.m_bSlowingTime || UTIL_TimeScaleIsNonStandard() ); }
#endif // USE_SLOWTIME

	void SetAirControlSupressionTime( float flDuration ) { m_PortalLocal.m_flAirControlSupressionTime = flDuration; }
	bool IsSuppressingAirControl( void ) { return m_PortalLocal.m_flAirControlSupressionTime != 0.0f; }

	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual void ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet );

	float GetImplicitVerticalStepSpeed() const;
	void SetImplicitVerticalStepSpeed( float speed );

	const CPortalPlayerLocalData& GetPortalPlayerLocalData() const;

	virtual Vector EyePosition();

#if USE_SLOWTIME
	// Time modulation
	void	StartSlowingTime( float flDuration );
	void	StopSlowingTime( void );
#endif // USE_SLOWTIME

	void	ShowViewFinder( void );
	void	HideViewFinder( void );

	// Coop ping effect
	void	PlayCoopPingEffect( void );

	CNetworkVar( bool, m_bPitchReorientation );
	CNetworkHandle( CPortal_Base2D, m_hPortalEnvironment ); //if the player is in a portal environment, this is the associated portal

	friend class CPortal_Base2D;

	virtual CBaseEntity* EntSelectSpawnPoint( void );
	void PickTeam( void );
	static void ClientDisconnected( edict_t *pPlayer );
	virtual void ChangeTeam( int iTeamNum );

	int FlashlightIsOn( void );
	bool FlashlightTurnOn( bool playSound /*= false*/ );
	void FlashlightTurnOff( bool playSound /*= false*/ );

	void SetInTractorBeam( CTrigger_TractorBeam *pTractorBeam );
	void SetLeaveTractorBeam( CTrigger_TractorBeam *pTractorBeam, bool bKeepFloating );
	CTrigger_TractorBeam* GetTractorBeam( void ) const { return m_PortalLocal.m_hTractorBeam.Get(); }

	friend class CPortalGameMovement;

	void PreventCrouchJump( CUserCmd* ucmd );

	void BridgeRemovedFromUnder( void );
	void WasDroppedByOtherPlayerWhileTaunting() { m_bWasDroppedByOtherPlayerWhileTaunting = true; }
	void OnPlayerLanded();

	void IncWheatleyMonitorDestructionCount( void ) { m_nWheatleyMonitorDestructionCount++; }
	int GetWheatleyMonitorDestructionCount( void ) { return m_nWheatleyMonitorDestructionCount; }

	void TurnOffPotatos( void ) { m_bPotatos = false; }
	void TurnOnPotatos( void ) { m_bPotatos = true; }

	void PlayCoopStepSound( const Vector& origin, int side, float volume );

protected:
	mutable Vector m_vWorldSpaceCenterHolder; //WorldSpaceCenter() returns a reference, need an actual value somewhere
	CNetworkVarEmbedded( CPortalPlayerLocalData, m_PortalLocal );

	CHandle< CNPC_SecurityCamera > m_hRemoteTauntCamera;

	void RespawnPlayer( void );

private:
	virtual const char *GetPlayerModelName( void );
	void	PlayUseDenySound( void );

public:
	void	Taunt( const char *pchTauntForce = NULL, bool bAuto = false );

private:
	bool	SolveTeamTauntPositionAndAngles( CPortal_Player *pInitiator );
	bool	ValidateTeamTaunt( CPortal_Player *pInitiator, Vector &vInitiatorPos, QAngle &angInitiatorAng, Vector &vAcceptorPos, QAngle &angAcceptorAng, bool bRecursed = false );
	void	StartTaunt( void );
	bool	FindRemoteTauntViewpoint( Vector *pOriginOut, QAngle *pAnglesOut );

#if USE_SLOWTIME
	CBaseEntity		*m_pSlowTimeColorFX;
#endif // USE_SLOWTIME

	CSoundPatch		*m_pWooshSound;
	CSoundPatch		*m_pGrabSound;

	int m_nWheatleyMonitorDestructionCount;

	CNetworkQAngleXYZ( m_angEyeAngles );

	CPortalPlayerAnimState*   m_PlayerAnimState;

	int m_iLastWeaponFireUsercmd;
	CNetworkVar( int, m_iSpawnInterpCounter );
	CNetworkVar( int, m_iPlayerSoundType );

	CUtlVector<ScriptedNPCInteraction_t> m_ScriptedInteractions;
	
	CNetworkVar( bool, m_bPingDisabled );
	CNetworkVar( bool, m_bTauntDisabled );
	CNetworkVar( bool, m_bTauntRemoteView );
	bool m_bTauntRemoteViewFOVFixup;
	CNetworkVector( m_vecRemoteViewOrigin );
	CNetworkQAngle( m_vecRemoteViewAngles );
	CNetworkVar( float, m_fTauntCameraDistance );
	CNetworkVar( int, m_nTeamTauntState );
	CNetworkVector( m_vTauntPosition );
	CNetworkQAngle( m_vTauntAngles );
	CNetworkQAngle( m_vPreTauntAngles );
	CNetworkVar( bool, m_bTrickFire );
	CNetworkHandle( CPortal_Player, m_hTauntPartnerInRange );
	CNetworkString( m_szTauntForce, PORTAL2_MP_TEAM_TAUNT_FORCE_LENGTH );

	CNetworkVar( bool, m_bHeldObjectOnOppositeSideOfPortal );
	CNetworkHandle( CPortal_Base2D, m_hHeldObjectPortal );	// networked entity handle

#if USE_SLOWTIME
	bool	m_bHasPlayedSlowTimeStopSound;
#endif // USE_SLOWTIME

	bool	m_bIntersectingPortalPlane;
	bool	m_bStuckOnPortalCollisionObject;
	bool	m_bPlayUseDenySound;				// Signaled by PlayerUse, but can be unset by HL2 ladder code...

	float	m_fNeuroToxinDamageTime;

	CNetworkVarEmbedded( PortalPlayerStatistics_t, m_StatsThisLevel );
	float m_fTimeLastNumSecondsUpdate;
	Vector m_vPrevPosition;

	int		m_iNumCamerasDetatched;

	// In multiplayer, last time we used a coop ping to draw our partner's attention
	float						m_flLastPingTime;

	// When a portal is placed in the same cluster as us, we flip this on
	// to signal the updating of the g_ClientCheck's cached PVS bits.
	bool						m_bClientCheckPVSDirty;

	float						m_flUseKeyCooldownTime;			// Disable use key until curtime >= this number

	CNetworkVar( bool, m_bIsHoldingSomething );

	float m_flImplicitVerticalStepSpeed;	// When moving with step code, the player has an implicit vertical
											// velocity that keeps her on ramps, steps, etc. We need this to
											// correctly transform her velocity when she teleports.

	// to check if the player just respawned on client
	CNetworkVar( bool, m_iSpawnCounter );

	// to check if the player was dropped from the bridge by the other player
	bool m_bWasDroppedByOtherPlayerWhileTaunting;

public: // PAINT SPECIFIC
	bool IsReorienting() const;
	Vector GetPaintGunShootPosition();

	bool IsPressingJumpKey() const;
	bool IsHoldingJumpKey() const;
	bool IsTryingToSuperJump( const PaintPowerInfo_t* pInfo = NULL ) const;
	void SetJumpedThisFrame( bool jumped );
	bool JumpedThisFrame() const;
	void SetBouncedThisFrame( bool bounced );
	bool BouncedThisFrame() const;
	InAirState GetInAirState() const;

	bool WantsToSwapGuns( void );
	void SetWantsToSwapGuns( bool bWantsToSwap );

	bool IsUsingPostTeleportationBox() const;

	const Vector& GetInputVector() const;
	void SetInputVector( const Vector& vInput );

	virtual Vector BodyTarget( const Vector &posSrc, bool bNoisy);

	const Vector& GetPrevGroundNormal() const;
	void SetPrevGroundNormal( const Vector& vPrevNormal );
	float PredictedAirTimeEnd( void );	// Uses the current velocity
	float PredictedBounce( void );		// Uses the current velocity
	void OnBounced( float fTimeOffset = 0.0f );

	virtual void SetFogController( CFogController *pFogController );

	virtual PaintPowerType GetPaintPowerAtPoint( const Vector& worldContactPt ) const;
	virtual void Paint( PaintPowerType type, const Vector& worldContactPt );
	virtual void CleansePaint();

	void Reorient( QAngle& viewAngles );
	float GetReorientationProgress() const;
	bool IsDoneReorienting() const;
	virtual const Vector GetPlayerMins() const;
	virtual const Vector GetPlayerMaxs() const;
	const Vector& GetHullMins() const;
	const Vector& GetHullMaxs() const;
	const Vector& GetStandHullMins() const;
	const Vector& GetStandHullMaxs() const;
	const Vector& GetDuckHullMins() const;
	const Vector& GetDuckHullMaxs() const;

	float GetHullHeight() const;
	float GetHullWidth() const;
	float GetStandHullHeight() const;
	float GetStandHullWidth() const;
	float GetDuckHullHeight() const;
	float GetDuckHullWidth() const;

	virtual void UpdateCollisionBounds();
	virtual void InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );

	bool PlayGesture( const char *pGestureName );

	void SetAirDuck( bool bDuckedInAir );

	StickCameraState GetStickCameraState() const;
	void SetQuaternionPunch( const Quaternion& qPunch );
	void DecayQuaternionPunch();

	using BaseClass::AddSurfacePaintPowerInfo;
	void AddSurfacePaintPowerInfo( const BrushContact& contact, char const* context = 0 );
	void AddSurfacePaintPowerInfo( const trace_t& trace, char const* context = 0 );

	void SetEyeUpOffset( const Vector& vOldUp, const Vector& vNewUp );
	void SetEyeOffset( const Vector& vOldOrigin, const Vector& vNewOrigin );

	void GivePlayerPaintGun( bool bActivatePaintPowers, bool bSwitchTo );
	void GivePlayerPortalGun( bool bUpgraded, bool bSwitchTo );
	void GivePlayerWearable( const char *pItemName );
	void RemovePlayerWearable( const char *pItemName );

	void ResetBounceCount() { m_nBounceCount = 0; }
	void ResetAirTauntCount() { m_nAirTauntCount = 0; }

	// Anim state code
	CNetworkVarEmbedded( CPortalPlayerShared, m_Shared );

	void SetHullHeight( float flHeight );

	virtual void ChooseActivePaintPowers( PaintPowerInfoVector& activePowers );

	bool IsFullyConnected() { return m_bIsFullyConnected; }
	void OnFullyConnected();

	void NetworkPortalTeleportation( CBaseEntity *pOther, CPortal_Base2D *pPortal, float fTime, bool bForcedDuck );

private: // PAINT SPECIFIC
	void DecayEyeOffset();

	void GivePortalPlayerItems( void );

	// Find all the contacts
	void DeterminePaintContacts();
	void PredictPaintContacts( const Vector& contactBoxMin,
		const Vector& contactBoxMax,
		const Vector& traceBoxMin,
		const Vector& traceBoxMax,
		float lookAheadTime,
		char const* context );
	void ChooseBestPaintPowersInRange( PaintPowerChoiceResultArray& bestPowers,
		PaintPowerConstIter begin,
		PaintPowerConstIter end,
		const PaintPowerChoiceCriteria_t& info ) const;

	// Paint Power User Implementation
	virtual PaintPowerState ActivateSpeedPower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState UseSpeedPower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState DeactivateSpeedPower( PaintPowerInfo_t& powerInfo );

	virtual PaintPowerState ActivateBouncePower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState UseBouncePower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState DeactivateBouncePower( PaintPowerInfo_t& powerInfo );

	void PlayPaintSounds( const PaintPowerChoiceResultArray& touchedPowers );
	void UpdatePaintedPower();
	void UpdateAirInputScaleFadeIn();
	void UpdateInAirState();
	void CachePaintPowerChoiceResults( const PaintPowerChoiceResultArray& choiceInfo );
	bool LateSuperJumpIsValid() const;
	void RecomputeBoundsForOrientation();
	void TryToChangeCollisionBounds( const Vector& newStandHullMin,
									 const Vector& newStandHullMax,
									 const Vector& newDuckHullMin,
									 const Vector& newDuckHullMax );

	float SpeedPaintAcceleration( float flDefaultMaxSpeed,
								  float flSpeed,
								  float flWishCos,
								  float flWishDirSpeed ) const;

	bool CheckToUseBouncePower( PaintPowerInfo_t& info );

	// stick camera
	void RotateUpVector( Vector& vForward, Vector& vUp );
	void SnapCamera( StickCameraState nCameraState, bool bLookingInBadDirection );
	void PostTeleportationCameraFixup( const CPortal_Base2D *pEnteredPortal );

	// Paint power debug
	void DrawJumpHelperDebug( PaintPowerConstIter begin, PaintPowerConstIter end, float duration, bool noDepthTest, const PaintPowerInfo_t* pSelected ) const;

	// PAINT POWER STATE
	PaintPowerInfo_t m_CachedJumpPower;
	Vector m_vInputVector;
	float m_flCachedJumpPowerTime;
	float m_flUsePostTeleportationBoxTime;
	float m_flSpeedDecelerationTime;
	float m_flPredictedJumpTime;
	bool m_bJumpWasPressedWhenForced;	// The jump button was actually pressed when ForceDuckThisFrame() was called
	int m_nBounceCount;	// Number of bounces in a row without touching the ground
	float m_LastGroundBouncePlaneDistance;
	float m_flLastSuppressedBounceTime;
	float m_flTimeSinceLastTouchedPower[3];
	int m_nPortalsEnteredInAirFlags;
	int m_nAirTauntCount;

	bool m_bIsFullyConnected;
	CNetworkVar( float,  m_flMotionBlurAmount );

	//Swapping guns
	CNetworkVar( bool, m_bWantsToSwapGuns );
	bool m_bSendSwapProximityFailEvent;

	CNetworkVar( bool, m_bPotatos );

	PlayerGunType m_PlayerGunType;
	PlayerGunType m_PlayerGunTypeWhenDead;
	bool m_bSpawnFromDeath;

	bool m_bIsBendy;

	Vector m_vPrevGroundNormal; // Our ground normal from the previous frame

	Vector m_vGravity;

	CNetworkVar( float, m_flHullHeight );

	//encoding these messages directly in the player to ensure it's received in sync with the corresponding entity post-teleport update
	//each player has their own copy of the buffer that is only sent to them
	CUtlVector<CEntityPortalledNetworkMessage> m_EntityPortalledNetworkMessages; 
	enum 
	{
		MAX_ENTITY_PORTALLED_NETWORK_MESSAGES = 32,
	};
	CNetworkVar( uint32, m_iEntityPortalledNetworkMessageCount ); //always ticks up by one per add

	//variables we'd like to persist between instances of grab controllers
	struct GrabControllerPersistentVars_t
	{
		CHandle<CPortal_Base2D> m_hOscillationWatch;
		CHandle<CPortal_Base2D> m_hLookingThroughPortalLastUpdate;
		Vector m_vLastTargetPosition;
		bool m_bLastUpdateWasForcedPull;

		void ResetOscillationWatch( void )
		{
			m_hOscillationWatch = NULL;
			m_hLookingThroughPortalLastUpdate = NULL;
			m_vLastTargetPosition.Init();
			m_bLastUpdateWasForcedPull = false;
		}
	};
	GrabControllerPersistentVars_t m_GrabControllerPersistentVars;

	struct RecentPortalTransform_t
	{
		int command_number;
		CHandle<CPortal_Base2D> Portal;
		matrix3x4_t matTransform;
	};

	CUtlVector<RecentPortalTransform_t> m_PendingPortalTransforms; //portal transforms we've sent to the client but they have not yet acknowledged, needed for some input fixup
	
	friend class CPortalPlayerShared;
	friend class CGrabController;

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

	//----------------------------
	// ECONOMY INVENTORY MANAGEMENT
public:
	// IInventoryUpdateListener
	virtual void InventoryUpdated( CPlayerInventory *pInventory );
	virtual void SOCacheUnsubscribed( const CSteamID & steamIDOwner ) { m_Shared.SetLoadoutUnavailable( true ); }

	void		UpdateInventory( bool bInit );
	void		VerifySOCache();

	// Inventory access
	CPortalPlayerInventory	*Inventory( void ) { return &m_Inventory; }
	CEconItemView *GetItemInLoadoutSlot( int iLoadoutSlot );

private:
	CPortalPlayerInventory	m_Inventory;

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

	bool m_bReadyForDLCItemUpdates;
};

inline CPortal_Player *ToPortalPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return assert_cast<CPortal_Player*>( pEntity );
}

inline CPortal_Player *GetPortalPlayer( int iPlayerIndex )
{
	return static_cast<CPortal_Player*>( UTIL_PlayerByIndex( iPlayerIndex ) );
}

#endif //PORTAL_PLAYER_H
