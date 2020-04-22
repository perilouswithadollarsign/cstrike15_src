//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef PORTAL_PLAYER_H
#define PORTAL_PLAYER_H
#pragma once

#include "portal_playeranimstate.h"
#include "c_baseplayer.h"
#include "portal_player_shared.h"
#include "c_portal_base2d.h"
#include "weapon_portalbase.h"
#include "colorcorrectionmgr.h"
#include "c_portal_playerlocaldata.h"
#include "iinput.h"
#include "paint_power_user.h"
#include "paintable_entity.h"
#include "portal2/portal_grabcontroller_shared.h"
#include "portal_shareddefs.h"

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	#include "portal2_item_inventory.h"
#endif

struct PaintPowerChoiceCriteria_t;

enum PortalScreenSpaceEffect
{
	PAINT_SCREEN_SPACE_EFFECT,

	PORTAL_SCREEN_SPACE_EFFECT_COUNT
};

class C_EntityPortalledNetworkMessage : public CMemZeroOnNew
{
public:	
	DECLARE_CLASS_NOBASE( C_EntityPortalledNetworkMessage );

	CHandle<C_BaseEntity> m_hEntity;
	CHandle<CPortal_Base2D> m_hPortal;
	float m_fTime;
	bool m_bForcedDuck;
	uint32 m_iMessageCount;
};

class CMoveData;

//=============================================================================
// >> Portal_Player
//=============================================================================
class C_Portal_Player : public PaintPowerUser< CPaintableEntity< C_BasePlayer > >
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	, public IInventoryUpdateListener
#endif
{
public:
	DECLARE_CLASS( C_Portal_Player, PaintPowerUser< CPaintableEntity< C_BasePlayer > > );

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();


	C_Portal_Player();
	~C_Portal_Player( void );

	virtual void UpdateOnRemove( void );
	virtual void Precache( void );

	virtual void PostThink();
	void ClientThink( void );

	static inline C_Portal_Player* GetLocalPortalPlayer( int nSlot = -1 )
	{
		return static_cast< C_Portal_Player* >( C_BasePlayer::GetLocalPlayer( nSlot ) );
	}

	static inline C_Portal_Player* GetLocalPlayer( int nSlot = -1 )
	{
		return static_cast< C_Portal_Player* >( C_BasePlayer::GetLocalPlayer( nSlot ) );
	}

	virtual Vector GetThirdPersonViewPosition( void );
	virtual const Vector& GetRenderOrigin( void );
	virtual const QAngle& GetRenderAngles();

	virtual void	SetAnimation( PLAYER_ANIM playerAnim );

	virtual void UpdateClientSideAnimation();
	void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	bool ShouldSkipRenderingViewpointPlayerForThisView( void );
	virtual const char *GetPlayerModelName( void );
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool Simulate( void );
	virtual IClientModelRenderable	*GetClientModelRenderable();

	QAngle GetAnimEyeAngles( void ) { return m_angEyeAngles; }
	Vector GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget = NULL );

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType( void );
	virtual C_BaseAnimating* BecomeRagdollOnClient();
	virtual bool			ShouldDraw( void );
	virtual bool			ShouldSuppressForSplitScreenPlayer( int nSlot );
	virtual PlayerRenderMode_t GetPlayerRenderMode( int nSlot );
	virtual void			GetRenderBoundsWorldspace( Vector& absMins, Vector& absMaxs );
	virtual const QAngle&	EyeAngles();
	virtual void			OnPreDataChanged( DataUpdateType_t type );
	virtual void			PreDataUpdate( DataUpdateType_t updateType );
	virtual void			OnDataChanged( DataUpdateType_t type );
	virtual void			PostDataUpdate( DataUpdateType_t updateType );
	virtual float			GetFOV( void );
	virtual CStudioHdr*		OnNewModel( void );
	virtual void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void			ItemPreFrame( void );
	virtual void			ItemPostFrame( void );
	virtual float			GetMinFOV()	const { return 5.0f; }
	virtual Vector			GetAutoaimVector( float flDelta );
	virtual bool			ShouldReceiveProjectedTextures( int flags );
	virtual void			GetStepSoundVelocities( float *velwalk, float *velrun );
	virtual void			PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void			PreThink( void );
	virtual void			DoImpactEffect( trace_t &tr, int nDamageType );
	virtual bool			CreateMove( float flInputSampleTime, CUserCmd *pCmd );
	virtual bool			ShouldCollide( int collisionGroup, int contentsMask ) const;
	virtual bool			IsZoomed( void )	{ return m_PortalLocal.m_bZoomedIn; }

	virtual const Vector	&WorldSpaceCenter() const;

	virtual Vector			EyePosition();
	virtual Vector			EyeFootPosition( const QAngle &qEyeAngles );//interpolates between eyes and feet based on view angle roll
	inline Vector			EyeFootPosition( void ) { return EyeFootPosition( EyeAngles() ); }; 
	void					PlayerPortalled( C_Portal_Base2D *pEnteredPortal, float fTime, bool bForcedDuck );
	void					CheckPlayerAboutToTouchPortal( void );

	bool					IsTaunting( void );
	int						GetTeamTauntState( void ) const { return m_nTeamTauntState; }
	bool					IsPingDisabled( void ) const { return m_bPingDisabled; }
	bool					IsTauntDisabled( void ) const { return m_bTauntDisabled; }
	bool					IsRemoteViewTaunt( void ) const { return m_bTauntRemoteView; }
	QAngle					GetTeamTauntAngles( void ) { return m_vTauntAngles; }
	QAngle					GetTrickFireAngles( void ) { return m_vPreTauntAngles; }
	bool					IsTrickFiring( void ) const { return m_bTrickFire; }
	void					ClearTrickFiring( void ) { m_bTrickFire = false; }
	bool					IsInterpolatingTauntAngles( void ) const { return m_bTauntInterpolatingAngles; }
	float					GetTauntCamTargetPitch( void ) const { return m_flTauntCamTargetPitch; }
	float					GetTauntCamTargetYaw( void ) const { return m_flTauntCamTargetYaw; }
	void					SetFaceTauntCameraEndAngles( bool bFaceTauntCameraEndAngles ) { m_bFaceTauntCameraEndAngles = bFaceTauntCameraEndAngles; }
	class C_Portal_Player*	HasTauntPartnerInRange( void ) const { return m_hTauntPartnerInRange.Get(); }
	const char*				GetTauntForceName( void ) const { return m_szTauntForce; }

	void					ResetHeldObjectOutOfEyeTransitionDT( void ) { m_flObjectOutOfEyeTransitionDT = 0.0f; }

	C_BaseEntity			*FindPlayerUseEntity( void );
	bool					IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );

	virtual void	CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	void			CalcPortalView( Vector &eyeOrigin, QAngle &eyeAngles, float &fov );
	virtual void	CalcViewModelView( const Vector& eyeOrigin, const QAngle& eyeAngles);

	bool			IsInvalidHandoff( CBaseEntity *pObject );
	void			PollForUseEntity( bool bBasicUse, CBaseEntity **ppUseEnt, CPortal_Base2D **ppUseThroughPortal );
	CBaseEntity*	FindUseEntity( C_Portal_Base2D **pThroughPortal );
	CBaseEntity*	FindUseEntityThroughPortal( void );

	inline bool		IsCloseToPortal( void ) //it's usually a good idea to turn on draw hacks when this is true
	{
		return ((PortalEyeInterpolation.m_bEyePositionIsInterpolating) || (m_hPortalEnvironment.Get() != NULL));	
	} 

	// Gesturing
	virtual bool StartSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );
	virtual bool StartGestureSceneEvent( CSceneEventInfo *info, CChoreoScene *scene, CChoreoEvent *event, CChoreoActor *actor, CBaseEntity *pTarget );
	
	void	SetAirControlSupressionTime( float flDuration ) { m_PortalLocal.m_flAirControlSupressionTime = flDuration; }
	bool	IsSuppressingAirControl( void ) { return m_PortalLocal.m_flAirControlSupressionTime != 0.0f; }

	void	UpdateLookAt( void );
	void	Initialize( void );

	virtual const char * GetVOIPParticleEffectName( void ) const;
	virtual CNewParticleEffect	*GetVOIPParticleEffect( void );

	C_Portal_Base2D *GetHeldObjectPortal( void ) const { return m_hHeldObjectPortal.Get(); }

	Activity TranslateActivity( Activity baseAct, bool *pRequired = NULL );
	CWeaponPortalBase* GetActivePortalWeapon() const;

#if USE_SLOWTIME
	bool	IsSlowingTime( void ) { return m_PortalLocal.m_bSlowingTime; }
#endif // USE_SLOWTIME
	bool	IsShowingViewFinder( void ) { return m_PortalLocal.m_bShowingViewFinder; }
	
	struct PredictedPortalTeleportation_t 
	{
		float flTime;
		C_Portal_Base2D *pEnteredPortal;
		int iCommandNumber;
		float fDeleteServerTimeStamp;
		bool bDuckForced;
		VMatrix matUnroll; //sometimes the portals move/fizzle between an apply and an unroll. Store the undo matrix ahead of time
	};
	CUtlVector<PredictedPortalTeleportation_t> m_PredictedPortalTeleportations;

	void FinishMove( CMoveData *move );

	bool	IsHoldingSomething( void ) const { return m_bIsHoldingSomething; }

	virtual void ApplyTransformToInterpolators( const VMatrix &matTransform, float fUpToTime, bool bIsRevertingPreviousTransform, bool bDuckForced );

	//single player doesn't predict portal teleportations. This is the call you'll receive when we determine the server portalled us.
	virtual void ApplyUnpredictedPortalTeleportation( const C_Portal_Base2D *pEnteredPortal, float flTeleportationTime, bool bForcedDuck );

	//WARNING: predicted teleportations WILL need to be undone A LOT. Prediction rolls time forward and backward like mad. Optimally an apply then undo should revert to the starting state. But an easier and somewhat acceptable solution is to have an undo then (assumed) re-apply be a NOP.
	virtual void ApplyPredictedPortalTeleportation( C_Portal_Base2D *pEnteredPortal, CMoveData *pMove, bool bForcedDuck );
	virtual void UndoPredictedPortalTeleportation( const C_Portal_Base2D *pEnteredPortal, float fOriginallyAppliedTime, const VMatrix &matUndo, bool bDuckForced ); //fOriginallyAppliedTime is the value of gpGlobals->curtime when ApplyPredictedPortalTeleportation was called. Which will be in the future when this gets called

	void UnrollPredictedTeleportations( int iCommandNumber ); //unroll all predicted teleportations at or after the target tick

	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	float GetImplicitVerticalStepSpeed() const;
	void SetImplicitVerticalStepSpeed( float speed );

	virtual void ForceDuckThisFrame( void );

	const C_PortalPlayerLocalData& GetPortalPlayerLocalData() const;

	bool	m_bPitchReorientation;
	float	m_fReorientationRate;
	bool	m_bEyePositionIsTransformedByPortal; //when the eye and body positions are not on the same side of a portal
	C_Portal_Base2D *m_pNoDrawForRecursionLevelOne; //if your eye is transformed by a portal, your body is directly on the other side of this portal

	CHandle<C_Portal_Base2D>	m_hPortalEnvironment; //a portal whose environment the player is currently in, should be invalid most of the time
	void FixPortalEnvironmentOwnership( void ); //if we run prediction, there are multiple cases where m_hPortalEnvironment != CPortalSimulator::GetSimulatorThatOwnsEntity( this ), and that's bad

	CGrabController &GetGrabController()
	{
		return m_GrabController;
	}

	void ToggleHeldObjectOnOppositeSideOfPortal( void ) { m_bHeldObjectOnOppositeSideOfPortal = !m_bHeldObjectOnOppositeSideOfPortal; }
	void SetHeldObjectOnOppositeSideOfPortal( bool p_bHeldObjectOnOppositeSideOfPortal ) { m_bHeldObjectOnOppositeSideOfPortal = p_bHeldObjectOnOppositeSideOfPortal; }
	bool IsHeldObjectOnOppositeSideOfPortal( void ) 
	{
		return m_bHeldObjectOnOppositeSideOfPortal; 
	}
	void SetHeldObjectPortal( CPortal_Base2D *pPortal ) { m_hHeldObjectPortal = pPortal; }
	void SetUsingVMGrabState( bool bState ) { m_bUsingVMGrabState = bState; }
	bool IsUsingVMGrab( void );
	bool WantsVMGrab( void );
	bool IsForcingDrop( void ) { return m_bForcingDrop; }

	EHANDLE m_hGrabbedEntity;
	EHANDLE m_hPortalThroughWhichGrabOccured;
	bool m_bSilentDropAndPickup;
	void ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis );
	void PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize );

	void SetInTractorBeam( CTrigger_TractorBeam *pTractorBeam );
	void SetLeaveTractorBeam( CTrigger_TractorBeam *pTractorBeam, bool bKeepFloating );
	C_Trigger_TractorBeam* GetTractorBeam( void ) const { return m_PortalLocal.m_hTractorBeam.Get(); }

	bool m_bForceFireNextPortal;

	void PreventCrouchJump( CUserCmd* ucmd );

	void BridgeRemovedFromUnder( void );

	float GetMotionBlurAmount( void ) { return m_flMotionBlurAmount; }

	friend class CPortalGameMovement;

	virtual int GetDefaultFOV( void ) const;

	void PlayCoopStepSound( const Vector& origin, int side, float volume );

protected:
	C_PortalPlayerLocalData		m_PortalLocal;

	mutable Vector m_vWorldSpaceCenterHolder; //WorldSpaceCenter() returns a reference, need an actual value somewhere

	bool PortalledMessageIsPending() const;

private:

	void AvoidPlayers( CUserCmd *pCmd );

	void ClientPlayerRespawn();

	// Taunting
	void TurnOnTauntCam( void );
	void TurnOffTauntCam( void );
	void TurnOffTauntCam_Finish();
	void TauntCamInterpolation();
	void HandleTaunting( void );

	bool m_bFaceTauntCameraEndAngles;
	bool m_bTauntInterpolating;
	bool m_bTauntInterpolatingAngles;
	float m_flTauntCamCurrentDist;
	float m_flTauntCamTargetDist;
	float m_flTauntCamTargetPitch;
	float m_flTauntCamTargetYaw;
	bool m_bFinishingTaunt;

	float m_flMotionBlurAmount;
	float m_flObjectOutOfEyeTransitionDT;

	C_Portal_Player( const C_Portal_Player & );

	void UpdatePortalEyeInterpolation( void );
	
	CPortalPlayerAnimState *m_PlayerAnimState;

	QAngle	m_angEyeAngles;
	CDiscontinuousInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	// we need to interpolate hull height to maintain the world space center
	float m_flHullHeight;
	CInterpolatedVar< float > m_iv_flHullHeight;

	CDiscontinuousInterpolatedVar< Vector > m_iv_vEyeOffset;

	virtual IRagdoll		*GetRepresentativeRagdoll() const;
	EHANDLE	m_hRagdoll;

	int	m_headYawPoseParam;
	int	m_headPitchPoseParam;
	float m_headYawMin;
	float m_headYawMax;
	float m_headPitchMin;
	float m_headPitchMax;

	float m_flPitchFixup;     // We always flip over 180, but we if we were already looking down then we don't need to go as far.
	float m_flUprightRotDist; // how far we need to flip over to get our feet back on the ground.

	bool m_isInit;
	Vector m_vLookAtTarget;

	float m_flLastBodyYaw;
	float m_flCurrentHeadYaw;
	float m_flCurrentHeadPitch;
	float m_flStartLookTime;

	CountdownTimer m_blinkTimer;

	int	  m_iSpawnInterpCounter;
	int	  m_iSpawnInterpCounterCache;

	int	  m_iPlayerSoundType;

	bool  m_bHeldObjectOnOppositeSideOfPortal;
	CHandle< C_Portal_Base2D > m_hHeldObjectPortal;

	struct PortalEyeInterpolation_t
	{
		bool	m_bEyePositionIsInterpolating; //flagged when the eye position would have popped between two distinct positions and we're smoothing it over
		Vector	m_vEyePosition_Interpolated; //we'll be giving the interpolation a certain amount of instant movement per frame based on how much an uninterpolated eye would have moved
		Vector	m_vEyePosition_Uninterpolated; //can't have smooth movement without tracking where we just were
		//bool	m_bNeedToUpdateEyePosition;
		//int		m_iFrameLastUpdated;

		int		m_iTickLastUpdated;
		float	m_fTickInterpolationAmountLastUpdated;
		bool	m_bDisableFreeMovement; //used for one frame usually when error in free movement is likely to be high
		bool	m_bUpdatePosition_FreeMove;

		PortalEyeInterpolation_t( void ) : m_iTickLastUpdated(0), m_fTickInterpolationAmountLastUpdated(0.0f), m_bDisableFreeMovement(false), m_bUpdatePosition_FreeMove(false) { };
	} PortalEyeInterpolation;

	struct PreDataChanged_Backup_t
	{
		CHandle<C_Portal_Base2D>	m_hPortalEnvironment;
		//Vector					m_ptPlayerPosition;
		QAngle					m_qEyeAngles;
		uint32					m_iEntityPortalledNetworkMessageCount;
	} PreDataChanged_Backup;

	bool				m_bPortalledMessagePending;				//Player portalled. It's easier to wait until we get a OnDataChanged() event or a CalcView() before we do anything about it. Otherwise bits and pieces can get undone
	VMatrix				m_PendingPortalMatrix;

	bool				m_bIsHoldingSomething;
	
	bool				m_bWasTaunting;
	bool				m_bGibbed;
	CameraThirdData_t	m_TauntCameraData;

	bool				m_bPingDisabled;
	bool				m_bTauntDisabled;
	bool				m_bTauntRemoteView;
	Vector				m_vecRemoteViewOrigin;
	QAngle				m_vecRemoteViewAngles;
	float				m_fTauntCameraDistance;
	
	float				m_fTeamTauntStartTime;
	int					m_nOldTeamTauntState;
	int					m_nTeamTauntState;
	Vector				m_vTauntPosition;
	QAngle				m_vTauntAngles;
	QAngle				m_vPreTauntAngles;
	bool				m_bTrickFire;
	CHandle<C_Portal_Player>	m_hTauntPartnerInRange;
	char				m_szTauntForce[ PORTAL2_MP_TEAM_TAUNT_FORCE_LENGTH ];

	QAngle				m_angTauntPredViewAngles;
	QAngle				m_angTauntEngViewAngles;

	int		m_nLastFrameDrawn;
	int		m_nLastDrawnStudioFlags;

	float	m_flUseKeyStartTime;	// for long duration uses, record the initial keypress start time
	int		m_nUseKeyEntFoundCommandNum;  // Kind of a hack... if we find a use ent, keep it around until it sends off to the server then clear
	int		m_nUseKeyEntClearCommandNum;
	int		m_nLastRecivedCommandNum;
	EHANDLE m_hUseEntToSend;		// if we find a use ent during the extended polling, keep the handle
	float	m_flAutoGrabLockOutTime;

	bool m_bForcingDrop;
	bool m_bUseVMGrab;
	bool m_bUsingVMGrabState;

	EHANDLE m_hAttachedObject;
	EHANDLE m_hOldAttachedObject;

	EHANDLE m_hPreDataChangedAttachedObject; // Ok, I just want to know if our attached object went null on this network update for some cleanup
											 // but the 'OldAttachedObject' above somehow got intertwined in some VM mode toggle logic I don't want to unravel.
											 // Adding yet another ehandle to the same entity so I can cleanly detect when the server has cleared our held 
											 // object irrespective of what held mode we are in.

	
	CGrabController m_GrabController;

public:
	QAngle m_vecCarriedObjectAngles;
	C_PlayerHeldObjectClone *m_pHeldEntityClone;
	C_PlayerHeldObjectClone *m_pHeldEntityThirdpersonClone;

	Vector m_vecCarriedObject_CurPosToTargetPos;
	QAngle m_vecCarriedObject_CurAngToTargetAng;

	//this is where we'll ease into the networked value over time and avoid applying newly networked data to previously predicted frames
	Vector m_vecCarriedObject_CurPosToTargetPos_Interpolated;
	QAngle m_vecCarriedObject_CurAngToTargetAng_Interpolated;
private:
	CInterpolatedVar<Vector> m_iv_vecCarriedObject_CurPosToTargetPos_Interpolator;
	CInterpolatedVar<QAngle> m_iv_vecCarriedObject_CurAngToTargetAng_Interpolator;

	EHANDLE m_hUseEntThroughPortal;
	bool	m_bUseWasDown;
	void PollForUseEntity( CUserCmd *pCmd );

	float m_flImplicitVerticalStepSpeed;	// When moving with step code, the player has an implicit vertical
											// velocity that keeps her on ramps, steps, etc. We need this to
											// correctly transform her velocity when she teleports.

	Vector m_vRenderOrigin;

	Vector m_vTempRenderOrigin;
	QAngle m_TempRenderAngles;

	bool m_iSpawnCounter;
	bool m_iOldSpawnCounter;

	float m_fLatestServerTeleport;
	VMatrix m_matLatestServerTeleportationInverseMatrix;

public: // PAINT SPECIFIC
	static bool RenderLocalScreenSpaceEffect( PortalScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h );

	virtual void SharedSpawn();
	virtual void Touch( CBaseEntity *pOther );

	virtual Vector Weapon_ShootPosition();

	virtual CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	void SelectItem( const char *pstr, int iSubType );

	bool IsPressingJumpKey() const;
	bool IsHoldingJumpKey() const;
	bool IsTryingToSuperJump( const PaintPowerInfo_t* pInfo = NULL ) const;
	void SetJumpedThisFrame( bool jumped );
	bool JumpedThisFrame() const;
	void SetBouncedThisFrame( bool bounced );
	bool BouncedThisFrame() const;
	InAirState GetInAirState() const;

	bool WantsToSwapGuns( void );

	bool IsPotatosOn( void );

	bool IsUsingPostTeleportationBox() const;

	const Vector& GetInputVector() const;
	void SetInputVector( const Vector& vInput );

	const Vector& GetPrevGroundNormal() const;
	void SetPrevGroundNormal( const Vector& vPrevNormal );

	Vector GetPaintGunShootPosition();

	EHANDLE GetAttachedObject ( void ) { return m_hAttachedObject; }

	virtual PaintPowerType GetPaintPowerAtPoint( const Vector& worldContactPt ) const;
	virtual void Paint( PaintPowerType type, const Vector& worldContactPt );
	virtual void CleansePaint();

	virtual bool RenderScreenSpaceEffect( PortalScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h );

	bool ScreenSpacePaintEffectIsActive() const;
	void SetScreenSpacePaintEffectColors( IMaterialVar* pColor1, IMaterialVar* pColor2 ) const;

	void Reorient( QAngle& viewAngles );
	float GetReorientationProgress() const;
	bool IsDoneReorienting() const;

	virtual void UpdateCollisionBounds();

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

	void SetAirDuck( bool bDuckedInAir );
	void UnDuck();

	StickCameraState GetStickCameraState() const;

	void SetQuaternionPunch( const Quaternion& qPunch );
	void DecayQuaternionPunch();

	using BaseClass::AddSurfacePaintPowerInfo;
	void AddSurfacePaintPowerInfo( const BrushContact& contact, char const* context = 0 );
	void AddSurfacePaintPowerInfo( const trace_t& trace, char const* context = 0 );

	void SetEyeOffset( const Vector& vOldOrigin, const Vector& vNewOrigin );

	bool IsInTeamTauntIdle( void );

	void SetHullHeight( float flHeight );

	virtual void					GetToolRecordingState( KeyValues *msg );

	float PredictedAirTimeEnd( void );	// Uses the current velocity
	float PredictedBounce( void );		// Uses the current velocity
	void OnBounced( float fTimeOffset = 0.0f );

	virtual void ChooseActivePaintPowers( PaintPowerInfoVector& activePowers );

private: // PAINT SPECIFIC
	void DecayEyeOffset();

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

	bool RenderScreenSpacePaintEffect( IMatRenderContext *pRenderContext );
	void InvalidatePaintEffects();

	bool CheckToUseBouncePower( PaintPowerInfo_t& info );

	// stick camera
	void RotateUpVector( Vector& vForward, Vector& vUp );
	void SnapCamera( StickCameraState nCameraState, bool bLookingInBadDirection );
	void PostTeleportationCameraFixup( const CPortal_Base2D *pEnteredPortal );

	// Paint power debug
	void DrawJumpHelperDebug( PaintPowerConstIter begin, PaintPowerConstIter end, float duration, bool noDepthTest, const PaintPowerInfo_t* pSelected ) const;
	void ManageHeldObject();

	void MoveHeldObjectOutOfPlayerEyes( void );

	// PAINT POWER STATE
	PaintPowerInfo_t m_CachedJumpPower;
	CUtlReference< CNewParticleEffect > m_PaintScreenSpaceEffect;
	// commenting out the 3rd person drop effect
	//CUtlReference< CNewParticleEffect > m_PaintDripEffect;
	CountdownTimer m_PaintScreenEffectCooldownTimer;
	Vector m_vInputVector;
	float m_flCachedJumpPowerTime;
	float m_flUsePostTeleportationBoxTime;
	float m_flSpeedDecelerationTime;
	float m_flPredictedJumpTime;
	bool m_bDoneStickInterp;
	bool m_bDoneCorrectPitch;
	bool m_bJumpWasPressedWhenForced;	// The jump button was actually pressed when ForceDuckThisFrame() was called

	float m_flTimeSinceLastTouchedPower[3];

	bool m_bDoneAirTauntHint;

	bool m_bWantsToSwapGuns;

	bool m_bPotatos;
	bool m_bIsBendy;

	Vector m_vPrevGroundNormal;	// Our ground normal from the previous frame

	bool m_bToolMode_EyeHasPortalled_LastRecord; //when recording, keep track of whether we teleported the camera position last capture or not. Need to avoid interpolating when switching

public:
	CPortalPlayerShared	m_Shared;

	// Coop effects
	void CreatePingPointer( Vector vecDestintaion );
	void DestroyPingPointer( void );
	CUtlReference< CNewParticleEffect >	m_FlingTrailEffect;		// the particle trail effect that shows behind bots in coop when they fling
	CUtlReference< CNewParticleEffect >	m_PointLaser;		// the pointer that point for when the player does some pointing
	bool m_bFlingTrailActive;
	bool m_bFlingTrailJustPortalled;
	bool m_bFlingTrailPrePortalled;

	PortalPlayerStatistics_t m_StatsThisLevel;

private:
	CUtlVector<C_EntityPortalledNetworkMessage> m_EntityPortalledNetworkMessages;
	enum 
	{
		MAX_ENTITY_PORTALLED_NETWORK_MESSAGES = 32,
	};
	uint32 m_iEntityPortalledNetworkMessageCount;

	friend class CPortalPlayerShared;
	friend class CMultiPlayerAnimState;

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

	//----------------------------
	// INVENTORY MANAGEMENT
public:
	// IInventoryUpdateListener
	virtual void InventoryUpdated( CPlayerInventory *pInventory );
	virtual void SOCacheUnsubscribed( const CSteamID & steamIDOwner ) { m_Shared.SetLoadoutUnavailable( true ); }
	void		 UpdateInventory( void );

	// Inventory access
	CPortalPlayerInventory	*Inventory( void ) { return &m_Inventory; }
	CEconItemView *GetItemInLoadoutSlot( int iLoadoutSlot ){ return m_Inventory.GetInventoryItemByItemID( m_EquippedLoadoutItemIndices[iLoadoutSlot] ); }

private:
	void	UpdateClientsideWearables( void );
	void	RemoveClientsideWearables( void );
	bool	ItemsMatch( CEconItemView *pCurItem, CEconItemView *pNewItem );

private:
	CPortalPlayerInventory	m_Inventory;
	bool					m_bInventoryReceived;

	// Items that have been equipped on this player instance (the inventory loadout may have changed)
	itemid_t				m_EquippedLoadoutItemIndices[LOADOUT_POSITION_COUNT];

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

	bool					m_bWasAlivePreUpdate;

};

inline C_Portal_Player *ToPortalPlayer( CBaseEntity *pEntity )
{
	C_BasePlayer *pPlayer = ToBasePlayer( pEntity );
	return assert_cast<C_Portal_Player *>( pPlayer );
}

inline C_Portal_Player *GetPortalPlayer( void )
{
	return static_cast<C_Portal_Player*>( C_BasePlayer::GetLocalPlayer() );
}

#endif //Portal_PLAYER_H
