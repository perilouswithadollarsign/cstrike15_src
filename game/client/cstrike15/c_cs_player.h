//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_CS_PLAYER_H
#define C_CS_PLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "cs_playeranimstate.h"
#include "c_baseplayer.h"
#include "cs_shareddefs.h"
#include "weapon_csbase.h"
#include "baseparticleentity.h"
#include "beamdraw.h"
#include "glow_outline_effect.h"

#include "cs_player_rank_shared.h"
#include "GameEventListener.h"

#include "cstrike15_item_inventory.h"

#include "cs_custom_weapon_visualsdata_processor.h"

#include "cs_player_shared.h"

#include "iinput.h"

#include "cstrike15_item_inventory.h"

#include "csgo_playeranimstate.h"

#include "cs_gamerules.h"

#include "basecsgrenade_projectile.h"

#define SPEECH_VOIP_PARTICLE_EFFECT_NAME "speech_voice"
#define RADIO_VOICE_PARTICLE_EFFECT_NAME "radio_voice"

enum CStrikeScreenSpaceEffect
{
	AR_LEADER_SCREEN_SPACE_EFFECT,

	CSTRIKE_SCREEN_SPACE_EFFECT_COUNT
};

//#define CHARACTER_SPAWN_CAM_ZOOM_IN
extern const char *musicTypeStrings[];

class C_PhysicsProp;

extern ConVar cl_disablefreezecam;

struct clientSmokeGrenadeRecord_t
{
	Vector m_vecPosition;
	float m_flInceptionTime;
	int m_iEntityId;
};

struct clientHitVerify_t
{

	clientHitVerify_t()
	{
		vecPosition = Vector(0, 0, 0);
		flTimestamp = 0;
		flExpireTime = -1;
	}

	clientHitVerify_t( Vector inPos, float flTimestamp, float flExpireTime )
	{
		vecPosition = inPos;
		flTimestamp = flTimestamp;
		flExpireTime = flExpireTime;
	}

	Vector	vecPosition;
	float	flTimestamp;
	float	flExpireTime;
};

class C_BulletHitModel : public C_BaseAnimating
{
	DECLARE_CLASS( C_BulletHitModel, C_BaseAnimating );

public:
	//C_BulletHitModel() : m_pPlayerParent(NULL), m_iBoneIndex( 0 ), m_bIsHit( false ), m_flTimeCreated( 0 ){}
	void			AttachToPlayer( C_CSPlayer *pTargetPlayer, int nBoneIndex, matrix3x4_t matLocalOffset, bool bIsHit, Vector vecStartPos );
	virtual int		DrawModel( int flags, const RenderableInstance_t &instance );
	bool			UpdatePosition( void );
	CCSPlayer *GetPlayerParent();
private:
	matrix3x4_t m_matLocal;
	int m_iBoneIndex;
	EHANDLE m_hPlayerParent;
	bool m_bIsHit;
	float m_flTimeCreated;
	Vector m_vecStartPos;
};


#define BONESNAPSHOT_ENTIRE_BODY 0
#define BONESNAPSHOT_UPPER_BODY 1

#define BONESNAPSHOT_DECAYDURATION_MIN 0
#define BONESNAPSHOT_DECAYDURATION_MAX 0.4f

class CBoneSnapshot
{
public:
	void			Update( CBaseAnimating* pEnt, bool bReadOnly = false );	
	inline void		UpdateReadOnly( void ) { Update( m_pEnt, true ); }

	void			SetShouldCapture( void )					{ m_bCapturePending = true; }

	void SetShouldCapture( float flNewDecayDuration )
	{
		m_bCapturePending = true;
		m_flDecayIdealDuration = clamp( flNewDecayDuration, BONESNAPSHOT_DECAYDURATION_MIN, BONESNAPSHOT_DECAYDURATION_MAX );
		m_flDecayStartTime = gpGlobals->curtime;
		m_flDecayEndTime = m_flDecayStartTime + m_flDecayIdealDuration;
	}

	bool			IsCapturePending( void )	{ return (m_bEnabled && m_bCapturePending); }

	void			Enable( void )				{ if ( !m_bEnabled ) { AbandonAnyPending(); } m_bEnabled = true; }
	void			Disable( void )				{ if ( m_bEnabled ) { AbandonAnyPending(); } m_bEnabled = false; }

	float			GetCurrentWeight( void )	{ return m_flWeight; }
	
	void			SetLastBoneSetupTimeIndex( void ) { m_flLastBoneSetupTimeIndex = gpGlobals->curtime; }
	
	void Init( void )
	{
		m_pEnt = NULL;
		m_flWeight = 0;
		memset( m_Weightlist, 0, ARRAYSIZE(m_Weightlist) );
		m_bWeightlistInitialized = false;
		m_bEnabled = false;
		m_bCapturePending = false;
		m_vecSubordinateSnapshots.RemoveAll();
		m_flDecayIdealDuration = 0.25f;
		m_flDecayStartTime = 0;
		m_flDecayEndTime = 0;
		m_vecWorldCapturePos.Init();
		m_flLastBoneSetupTimeIndex = 0;
		m_nStudioRenderHdrId = -1;
	}

	void Init( CBaseAnimating* pEnt )			{ Init(); m_pEnt = pEnt; }
	CBoneSnapshot() { Init(); }

	void AddSubordinate( CBoneSnapshot* pSnapshot )
	{
		if ( !m_vecSubordinateSnapshots.HasElement(pSnapshot) )
		{
			m_vecSubordinateSnapshots[ m_vecSubordinateSnapshots.AddToTail() ] = pSnapshot;
		}
	}

	void AbandonAnyPending( void )
	{
		m_flWeight = 0;
		m_bCapturePending = false;
	}

	void SetWeightListName( const char* szWeightListName ) { m_szWeightlistName = szWeightListName; }

private:

	CBaseAnimating* m_pEnt;
	float			m_flWeight;
	matrix3x4a_t	m_Cache[ MAXSTUDIOBONES ];
	const char*		m_szWeightlistName;
	float			m_Weightlist[ MAXSTUDIOBONES ];
	bool			m_bWeightlistInitialized;
	bool			m_bEnabled;
	bool			m_bCapturePending;
	float			m_flDecayIdealDuration;
	float			m_flDecayStartTime;
	float			m_flDecayEndTime;
	Vector			m_vecWorldCapturePos;
	float			m_flLastBoneSetupTimeIndex;
	int				m_nStudioRenderHdrId;

	CUtlVector<CBoneSnapshot*> m_vecSubordinateSnapshots;

	void			CaptureSnapshot( void );
	void			PlaybackSnapshot( void );
	void			InitWeightList( void );
	bool			IsBoneSetupTimeIndexRecent( void ) { return (gpGlobals->curtime - m_flLastBoneSetupTimeIndex) <= 0.25f; }
};

//-----------------------------------------------------------------------------
class CAddonModel
{
public:
	CHandle<C_BaseAnimating> m_hEnt;	// The model for the addon.
	int m_iAddon;						// One of the ADDON_ bits telling which model this is.
	int m_iAttachmentPoint;				// Which attachment point on the player model this guy is on.
};

// Class that handles the playback of a sound instance during round start
class RoundStartSoundPlaybackData
{
public:
	RoundStartSoundPlaybackData() { m_pEntityPlayingSound = NULL; m_SoundName[0] = 0; m_fPlaybackTime = 0.0f; m_bHasBeenPlayed = false; }

	CBaseEntity*	m_pEntityPlayingSound;	// Entity playing the sound event
	char			m_SoundName[64];		// Name of sound event to play
	float			m_fPlaybackTime;		// Length of time for sound event to play
	bool			m_bHasBeenPlayed;		// true = event has been played
};

class CStartOfRoundAudioPlayback
{
public:
	CStartOfRoundAudioPlayback() { m_NextAvailableTime = 1.0f; m_bPlaybackEnabled = false; }

	// Adds a sound event to be played at the next round restart
	void	AddSound( CBaseEntity* pEntityPlayingSound, char* pName, float fDelayTime );

	// Play all the queued sounds
	void	PlaySounds( void );

	// Clear all the queued sounds
	void	ClearSounds( void );

	// Updates the playback when m_bPlaybackEnabled is valid
	void	Update( void );

	void	RemoveSoundByName( char* pName );

protected:
	// List of sounds to play at start of round
	CUtlVector<RoundStartSoundPlaybackData>	m_SoundEvents;

	// Time from start of round when the next added sound event can play
	float	m_NextAvailableTime;

	// Allows Update() to be called so sounds can play
	bool	m_bPlaybackEnabled;
};


//-----------------------------------------------------------------------------
class C_CSPlayer : public C_BasePlayer, public CGameEventListener, public ICSPlayerAnimStateHelpers
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	, public IHasAttributes
#endif
{
public:
	DECLARE_CLASS( C_CSPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	C_CSPlayer();
	~C_CSPlayer();

	float *	GetRenderClipPlane( void );
	float	m_fRenderingClipPlane[4];
	int		m_nLastClipPlaneSetupFrame;
	int		m_nClipPlaneProximityLimitAttachmentIndex;
	Vector	m_vecLastClipCameraPos;
	Vector	m_vecLastClipCameraForward;
	bool	m_bClipHitStaticWorld;
	bool	m_bCachedPlaneIsValid;
	CBaseWeaponWorldModel *m_pClippingWeaponWorldModel;

	virtual void SetSequence(int nSequence) OVERRIDE;

	virtual bool Simulate();
	virtual	void Spawn( void );
	virtual void UpdateOnRemove( void );
	virtual void OnSetDormant( bool bDormant );

	//EHANDLE GetCarriedHostage() const;
	void GiveCarriedHostage( EHANDLE hHostage );
	void RefreshCarriedHostage( bool bForceCreate );
	void RemoveCarriedHostage();

	bool HasDefuser() const;

	void GiveDefuser();
	void RemoveDefuser();

	bool HasNightVision() const;

	static C_CSPlayer* GetLocalCSPlayer();
	CSPlayerState State_Get() const;
	CSPlayerState m_previousPlayerState;

	virtual float GetMinFOV() const;
	
	// Get how much $$$ this guy has.
	int GetAccount() const;

	// Returns one of the CS_CLASS_ enums.
	int PlayerClass() const;

	bool IsInBuyZone();
	bool IsInBuyPeriod();
	bool CanBuyDuringImmunity();
	bool CanPlayerBuy( bool display );

	bool CanShowTeamMenu() const;	// Returns true if we're allowed to show the team menu right now.

	// Get the amount of armor the player has.
	int ArmorValue() const;
	bool HasHelmet() const;
	bool HasHeavyArmor() const;

	// create tracers
	void CreateWeaponTracer( Vector vecStart, Vector vecEnd );

	virtual const QAngle& EyeAngles();
	virtual void CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );

	// observer mode
	void				SetSpecWatchingGrenade( C_BaseEntity *pGrenade, bool bWatching );
	virtual int			GetObserverMode() const;
	virtual CBaseEntity	*GetObserverTarget() const;
	bool				IsSpecFollowingGrenade( void ) {return m_bIsSpecFollowingGrenade; }
	bool				IsHoldingSpecGrenadeKey( void );
	bool	m_bIsSpecFollowingGrenade;
	EHANDLE			m_hOldGrenadeObserverTarget;

	virtual void			GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual void			GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );
	virtual bool			GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );

	// Get the ID target entity index. The ID target is the player that is behind our crosshairs, used to
	// display the player's name.
	int GetIDTarget() const;
	int GetTargetedWeapon( void ) const;

	virtual void NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual void ClientThink();
	virtual void OnTimeJump() OVERRIDE;
	
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual bool Interpolate( float currentTime );
	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual surfacedata_t * GetFootstepSurface( const Vector &origin, const char *surfaceName );
	virtual void PlayClientJumpSound( void );
	virtual void ValidateModelIndex( void );

	static bool RenderLocalScreenSpaceEffect( CStrikeScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h );
	bool AreScreenSpaceVomitParticlesActive( void ) const;
	bool DrawScreenSpaceVomitParticles( IMatRenderContext *pRenderContext );

	int		GetHealth() const		{ return m_iHealth; }
	virtual int	GetMaxHealth() const;

	bool		Weapon_CanSwitchTo(C_BaseCombatWeapon *pWeapon);

	virtual void UpdateClientSideAnimation();
	virtual void ProcessMuzzleFlashEvent();
	void HandleTaserAnimation();
	
	virtual const Vector& GetRenderOrigin( void );
		
	bool CreateMove( float flInputSampleTime, CUserCmd *pCmd );

	CUtlVector< C_BaseParticleEntity* > m_SmokeGrenades;

	virtual bool ShouldDraw( void );
	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence ) OVERRIDE;
	virtual void AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	bool UpdateLayerWeaponDispatch( C_AnimationLayer *pLayer, int iSequence );
	virtual float GetLayerSequenceCycleRate( C_AnimationLayer *pLayer, int iSequence );
	virtual void BuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );

	virtual void DoExtraBoneProcessing( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternion q[], matrix3x4a_t boneToWorld[], CBoneBitList &boneComputed, CIKContext *pIKContext ) OVERRIDE;
	
	virtual bool SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );

	void ReevauluateAnimLOD( int boneMask = 0 );

	virtual C_BaseAnimating * BecomeRagdollOnClient();
	virtual IRagdoll* GetRepresentativeRagdoll() const;
	
	virtual void TeamChange( int iNewTeam );

	void ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName );

	// Have this player play the sounds from his view model's reload animation.
	void PlayReloadEffect( int iActAnimID, const Vector *pOrigin = NULL );

	virtual void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	virtual void DropPhysicsMag( const char *options ) OVERRIDE;

	bool		 HasC4( void );
	bool HasWeaponOfType( int nWeaponID ) const;

	virtual bool CreateLightEffects( void ) { return false; }	//no dimlight effects

	// Sometimes the server wants to update the client's cycle to get the two to run in sync (for proper hit detection)
	virtual void SetServerIntendedCycle( float intended ) { m_serverIntendedCycle = intended; }
	virtual float GetServerIntendedCycle( void ) { return m_serverIntendedCycle; }

	virtual bool IsTaunting( void ) const { return m_bIsTaunting; }
	virtual bool IsThirdPersonTaunt( void ) const { return m_bIsThirdPersonTaunt; }
	virtual bool IsHoldingTaunt( void ) const { return m_bIsHoldingTaunt; }

	virtual bool IsLookingAtWeapon( void ) const { return m_bIsLookingAtWeapon; }
	virtual bool IsHoldingLookAtWeapon( void ) const { return m_bIsHoldingLookAtWeapon; }

	virtual float GetTauntYaw( void ) const { return m_flTauntYaw; }

	virtual const char			*GetVOIPParticleEffectName() const { return SPEECH_VOIP_PARTICLE_EFFECT_NAME; }
	virtual const char			*GetRadioHeadParticleEffectName() const { return RADIO_VOICE_PARTICLE_EFFECT_NAME; }
	virtual void				UpdateRadioHeadIcon( bool bVoice );
	virtual bool				ShouldShowRadioHeadIcon() const;
	virtual CNewParticleEffect	*GetRadioHeadParticleEffect( void );
	virtual CNewParticleEffect	*GetVOIPParticleEffect( void );
	virtual Vector				GetParticleHeadLabelOffset( void );
	virtual bool				HasSpeechVOIPParticleIconActive( void ) { return (m_speechVOIPParticleEffect && m_speechVOIPParticleEffect.IsValid()); }

	virtual void AddDecal( const Vector& rayStart, const Vector& rayEnd, const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal );

	virtual void SetRenderAlpha( byte a );
	virtual void SetRenderMode( RenderMode_t nRenderMode, bool bForceUpdate = false );

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	virtual bool ShouldReceiveProjectedTextures( int flags )
	{
		return ( this != C_BasePlayer::GetLocalPlayer() || IsTaunting() || ShouldDraw() );
	}

	void ClearSoundEvents()
	{
		m_SoundEvents.RemoveAll();
	}

	CsMusicType_t GetCurrentMusic() { return m_nCurrentMusic; } const
	void SetCurrentMusic( CsMusicType_t nMusicType )
	{
		m_nCurrentMusic = nMusicType;
 		if( nMusicType == CSMUSIC_START )
 		{
			m_flMusicRoundStartTime = gpGlobals->curtime;
		}
		m_flCurrentMusicStartTime = gpGlobals->curtime;
	}
	float GetCurrentMusicElapsed()
	{
		return  gpGlobals->curtime - m_flCurrentMusicStartTime;
	}

	float GetMusicStartRoundElapsed()
	{
		return  gpGlobals->curtime - m_flMusicRoundStartTime;
	}

	bool IsPlayerDominated( int iPlayerIndex );
	bool IsPlayerDominatingMe( int iPlayerIndex );

	virtual float				GetFOV( void ) const;	
	
	virtual void CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	virtual void CalcDeathCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov );
	
	void UpdateFreezeCamFlashlightEffect( C_BaseEntity *pTarget, float flAmount );
	void CancelFreezeCamFlashlightEffect();
	bool m_bFreezeCamFlashlightActive;
	CTextureReference m_freezeCamSpotLightTexture;

	virtual float GetDeathCamInterpolationTime();
	float GetFreezeFrameInterpolant( void );
	float GetFreezeFrameTilt( void ) { return m_flFreezeFrameTilt; }

	// check autoaim
	bool IsCursorOnAutoAimTarget();

	AcquireResult::Type CanAcquire( CSWeaponID weaponId, AcquireMethod::Type acquireMethod, CEconItemView *pItem = NULL );
	int					GetCarryLimit( CSWeaponID weaponId );
	int					GetWeaponPrice( CSWeaponID weaponId, const CEconItemView *pWepView = NULL ) const;
	CWeaponCSBase*		CSWeapon_OwnsThisType( CEconItemView *pItem ) const;

	virtual bool ShouldRegenerateOriginFromCellBits() const;

	bool IsOtherSameTeam( int nTeam );
	bool IsOtherEnemy( CCSPlayer *pPlayer );
	bool IsOtherEnemy( int nEntIndex );
	bool IsOtherEnemyAndPlaying( int nEntIndex );

	virtual void SetModelPointer( const model_t *pModel );

protected:
	CUtlReference< CNewParticleEffect > m_radioHeadIconParticleEffect;

// Called by shared code.
public:

	// ICSPlayerAnimState overrides.
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon();
	virtual bool CSAnim_CanMove();


	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

		
// Implemented in shared code.
public:	
	virtual float GetPlayerMaxSpeed();
	bool IsPrimaryOrSecondaryWeapon( CSWeaponType nType );

	bool GetUseConfigurationForHighPriorityUseEntity( CBaseEntity *pEntity, CConfigurationForHighPriorityUseEntity_t &cfg );
	CBaseEntity *GetUsableHighPriorityEntity( void );

	void GetBulletTypeParameters( 
		int iBulletType, 
		float &fPenetrationPower, 
		float &flPenetrationDistance );

	void FireBullet(
		Vector vecSrc,
		const QAngle &shootAngles,
		float flDistance,
		float flPenetration,
		int nPenetrationCount,
		int iBulletType,
		int iDamage,
		float flRangeModifier,
		CBaseEntity *pevAttacker,
		bool bDoEffects,
		float xSpread, float ySpread );

	bool HandleBulletPenetration( 
		float &flPenetration,
		int &iEnterMaterial,
		bool &hitGrate,
		trace_t &tr,
		Vector &vecDir,
		surfacedata_t *pSurfaceData,
		float flPenetrationModifier,
		float flDamageModifier,
		bool bDoEffects,
		int iDamageType,
		float flPenetrationPower,
		int &nPenetrationCount,
		Vector &vecSrc,
		float flDistance,
		float flCurrentDistance,
		float &fCurrentDamage );

	virtual QAngle	GetAimPunchAngle( void );
	QAngle	GetRawAimPunchAngle( void ) const;

	void KickBack(
		float fAngle,
		float fMagnitude );

	int GetDefaultCrouchedFOV( void ) const;

	void DisplayPenetrationDebug( Vector vecEnter, Vector vecExit, float flDistance, float flInitialDamage, float flDamageLostImpact, float flTotalLostDamage, short nEnterSurf, short nExitSurf );

	// Returns true if the player is allowed to move.
	bool CanMove() const;
	
	// Returns the player mask which includes the solid mask plus the team mask.
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

	void OnJump( float fImpulse );
	void OnLand( float fVelocity );

	bool HasC4() const;	// Is this player carrying a C4 bomb?
	bool IsVIP() const;	// Is this player the VIP?

	virtual void SetAnimation( PLAYER_ANIM playerAnim );
	
	virtual bool ShouldInterpolate( void );

public:

	void UpdateIDTarget( void );
	void UpdateTargetedWeapon( void );
	void RemoveAddonModels( void );
	void UpdateMinModels( void );

	void CreateC4Effect( CBaseEntity *pEnt, bool bIsWeaponModel );
	void RemoveC4Effect( bool bIsWeaponModel );

	void SetActivity( Activity eActivity );
	Activity GetActivity( void ) const;

	void DoAnimStateEvent( PlayerAnimEvent_t evt );

	// Global/static methods
	virtual void ThirdPersonSwitch( bool bThirdperson );
	virtual void CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );

public:

	IPlayerAnimState *m_PlayerAnimState;
	CCSGOPlayerAnimState *m_PlayerAnimStateCSGO;

	virtual Vector Weapon_ShootPosition();

	// Used to control animation state.
	Activity m_Activity;

	CNetworkVar( bool, m_bIsScoped );
	CNetworkVar( bool, m_bIsWalking );
	// Predicted variables.
	CNetworkVar( bool, m_bResumeZoom );
	CNetworkVar( CSPlayerState, m_iPlayerState );	// SupraFiend: this gives the current state in the joining process, the states are listed above
	CNetworkVar( bool, m_bIsDefusing );			// tracks whether this player is currently defusing a bomb
	CNetworkVar( bool, m_bIsGrabbingHostage );			// tracks whether this player is currently defusing a bomb
	CNetworkVar( bool, m_bIsRescuing );			// tracks whether this player is currently rescuing a hostage
	CNetworkVar( float, m_fImmuneToGunGameDamageTime );	// When gun game spawn damage immunity will expire
	float m_fImmuneToGunGameDamageTimeLast;
	CNetworkVar( bool, m_bGunGameImmunity );	// tracks whether this player is currently immune in gun game
	CNetworkVar( bool, m_bHasMovedSinceSpawn ); // Whether player has moved from spawn position
	CNetworkVar( bool, m_bMadeFinalGunGameProgressiveKill );
	CNetworkVar( int, m_iGunGameProgressiveWeaponIndex ); // index of current gun game weapon
	CNetworkVar( int, m_iNumGunGameTRKillPoints );	// number of kill points accumulated so far in TR Gun Game mode (resets to 0 when weapon is upgraded)
	CNetworkVar( int, m_iNumGunGameKillsWithCurrentWeapon );
	CNetworkVar( int, m_iNumRoundKills );	// number of kills a player has gotten in a round
	CNetworkVar( int, m_iNumRoundKillsHeadshots );	// number of headshot kills a player has gotten in a round
	CNetworkVar( float, m_fMolotovUseTime );	// Molotov can be used if current time is after this time
	CNetworkVar( float, m_fMolotovDamageTime );	// Last time when this player was burnt by Molotov damage
	CNetworkVar( bool, m_bInBombZone );
	CNetworkVar( bool, m_bInBuyZone );
	CNetworkVar( bool, m_bInNoDefuseArea );
	CNetworkVar( int, m_iThrowGrenadeCounter );	// used to trigger grenade throw animations.
	CNetworkVar( bool, m_bWaitForNoAttack );	// flag to indicate player cannot attack until the attack button is released
	CNetworkVar( bool, m_bIsRespawningForDMBonus ); // flag to indicate player wants to spawn with the deathmatch bonus weapons
	CNetworkVar( float, m_flGuardianTooFarDistFrac );
	CNetworkVar( float, m_flDetectedByEnemySensorTime );
	float m_flNextGuardianTooFarWarning;

	CNetworkVar( bool, m_bKilledByTaser );
	CNetworkVar( int, m_iMoveState );		// Is the player trying to run or walk or idle?  Tells us what the player is "trying" to do.

	CNetworkVar( bool, m_bCanMoveDuringFreezePeriod );

	CNetworkVar( bool, m_isCurrentGunGameLeader );
	CNetworkVar( bool, m_isCurrentGunGameTeamLeader );
		
	CNetworkString( m_szArmsModel, MAX_MODEL_STRING_SIZE );		// Which arms we're using for the view model.
	
	const PlayerViewmodelArmConfig *m_pViewmodelArmConfig;

	CNetworkVar( float, m_flLowerBodyYawTarget );
	CNetworkVar( bool, m_bStrafing );

	bool m_bUseNewAnimstate;

	CBoneSnapshot m_boneSnapshots[2];
	bool IsAnyBoneSnapshotPending( void );

	float m_flLastSpawnTimeIndex;

	virtual void NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence ) OVERRIDE;
	virtual void NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight ) OVERRIDE;
	virtual void NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewCycle ) OVERRIDE;

	bool IsInHostageRescueZone( void );

	// This is a combination of the ADDON_ flags in cs_shareddefs.h.
	CNetworkVar( int, m_iAddonBits );

	// Clients don't know about holstered weapons, so we need to be told about them here
	CNetworkVar( int, m_iPrimaryAddon );
	CNetworkVar( int, m_iSecondaryAddon );

	// How long the progress bar takes to get to the end. If this is 0, then the progress bar
	// should not be drawn.
	CNetworkVar( int, m_iProgressBarDuration );
	
	// When the progress bar should start.
	CNetworkVar( float, m_flProgressBarStartTime );

	CNetworkVar( float, m_flStamina );
	CNetworkVar( int, m_iDirection );	// The current lateral kicking direction; 1 = right,  0 = left
	CNetworkVar( int, m_iShotsFired );	// number of shots fired recently
	CNetworkVar( int, m_nNumFastDucks ); // UNUSED.  Kept for backwards demo compatibility.  $$$REI TODO: Investigate safely removing variables
	CNetworkVar( bool, m_bDuckOverride ); // force the player to duck regardless of if they're holding crouch
	
	CNetworkVar( bool, m_bNightVisionOn );
	CNetworkVar( bool, m_bHasNightVision );
	CNetworkVar( float, m_flVelocityModifier );
	CNetworkVar( float, m_flGroundAccelLinearFracLastTime );

	CNetworkVar( int, m_iStartAccount );		// how much cash the player started the round with

	CNetworkVar( int, m_totalHitsOnServer );

	CNetworkVar( bool, m_bIsBuyMenuOpen );
	bool IsBuyMenuOpen( void ) { return m_bIsBuyMenuOpen; } 
	void SetBuyMenuOpen( bool bOpen ); 

	EHANDLE	m_hRagdoll;

	EHANDLE	m_hCarriedHostage;
	EHANDLE	m_hCarriedHostageProp;
	bool	m_bPlayingHostageCarrySound;

	CWeaponCSBase* GetActiveCSWeapon() const;
	CWeaponCSBase* GetCSWeapon( CSWeaponID id ) const;

	virtual ShadowType_t		ShadowCastType();

#ifdef CS_SHIELD_ENABLED
	bool HasShield( void ) { return m_bHasShield; }
	bool IsShieldDrawn( void ) { return m_bShieldDrawn;	}
	void SetShieldDrawnState( bool bState ) { m_bShieldDrawn = bState; }
#else
	bool HasShield( void ) { return false; }
	bool IsShieldDrawn( void ) { return false; }
	void SetShieldDrawnState( bool bState ) {}
#endif
	
	bool IsMiniScoreHidden( void ) { return m_bHud_MiniScoreHidden; }
	bool IsRadarHidden( void ) { return m_bHud_RadarHidden; }

	
	virtual int GetLastKillerIndex( void ) { return m_nLastKillerIndex; }
	int GetLastConcurrentKilled( void ) { return m_nLastConcurrentKilled; }
	int GetDeathCamMusicIndex( void ) { return m_nDeathCamMusic; }

	float m_flNightVisionAlpha;

	float m_flFlashBangTime;		// end time
	float m_flFlashScreenshotAlpha;
	float m_flFlashOverlayAlpha;
	bool m_bFlashBuildUp;
	bool m_bFlashDspHasBeenCleared;
	bool m_bFlashScreenshotHasBeenGrabbed;

	bool IsFlashBangActive( void ) { return ( m_flFlashDuration > 0.0f ) && ( gpGlobals->curtime < m_flFlashBangTime ); }
	bool IsFlashBangBuildUpActive( void ) { return m_bFlashBuildUp && IsFlashBangActive(); }
	float GetFlashStartTime( void ) { return (m_flFlashBangTime - m_flFlashDuration); }
	float GetFlashTimeElapsed( void ) { return MAX( gpGlobals->curtime - GetFlashStartTime(), 0.0f ); }

	bool IsBlinded( void ) { return (m_flFlashBangTime - 1.0f) > gpGlobals->curtime; }
	CNetworkVar( float, m_flFlashMaxAlpha );
	CNetworkVar( float, m_flFlashDuration );	

	float GetLastFiredWeaponTime( void ) { return m_flLastFiredWeaponTime; }
	float m_flLastFiredWeaponTime;

	// Having the RecvProxy in the player allows us to keep the var private
	static void RecvProxy_CycleLatch( const CRecvProxyData *pData, void *pStruct, void *pOut );

	// Bots and hostages auto-duck during jumps
	bool m_duckUntilOnGround;

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	void SurpressLadderChecks( const Vector& pos, const Vector& normal );
	bool CanGrabLadder( const Vector& pos, const Vector& normal );

	// [tj] checks if this player has another given player on their Steam friends list.
	bool HasPlayerAsFriend( C_CSPlayer* player );

	bool MadeFinalGunGameProgressiveKill( void ) { return m_bMadeFinalGunGameProgressiveKill; }
	int GetPlayerGunGameWeaponIndex( void ) { return m_iGunGameProgressiveWeaponIndex; }
	int GetNumGunGameTRKillPoints( void ) { return m_iNumGunGameTRKillPoints; }
	int GetNumGunGameKillsWithCurrentWeapon( void ) { return m_iNumGunGameKillsWithCurrentWeapon; }
	
	int GetNumRoundKills( void ) { return m_iNumRoundKills; }
	int GetNumRoundKillsHeadshots( void ) { return m_iNumRoundKillsHeadshots; }

	void DisplayInventory(bool showPistol);
	
	bool CanUseGrenade( CSWeaponID );

	MedalRank_t GetRank( MedalCategory_t category );

	uint32 GetMusicID( void );

	void ToggleRandomWeapons( void );

	bool ShouldShowTeamPlayerColors( int nOtherTeamNum );
	bool ShouldShowTeamPlayerColorLetters( void );

	static void UpdateGlowsForAllPlayers( void );
	CGlowObject m_GlowObject;

	uint16 GetCurrentEquipmentValue ( void ) const { return m_unCurrentEquipmentValue; }
	uint16 GetRoundStartEquipmentValue ( void ) const { return m_unRoundStartEquipmentValue; }
	uint16 GetFreezetimeEndEquipmentValue ( void ) const { return m_unFreezetimeEndEquipmentValue; }

	bool IsAbleToInstantRespawn( void );
	char const * IsAbleToApplySpray( trace_t *ptr, Vector *pvecForward, Vector *pvecRight );

	float GetLastSmokeOverlayAlpha( void ) const { return m_flLastSmokeOverlayAlpha; }

	char *GetHalloweenMaskModelAddon( C_CSPlayer *pPlayer );

	uint32 GetActiveQuestID( void ) const;
	QuestProgress::Reason GetQuestProgressReason( void ) const;

	Vector m_vecLastMuzzleFlashPos;
	QAngle m_angLastMuzzleFlashAngle;
	EHANDLE m_hMuzzleFlashShape;
	int m_iHealthBarRenderMaskIndex;
	float m_flHealthFadeValue;
	float m_flHealthFadeAlpha;

	// Match Stats data
	int m_iMatchStats_Kills[ MAX_MATCH_STATS_ROUNDS ];				//kills, per round
	int m_iMatchStats_Damage[ MAX_MATCH_STATS_ROUNDS ];				//damage, per round
	int m_iMatchStats_EquipmentValue[ MAX_MATCH_STATS_ROUNDS ];				//Equipment value, per round
	int m_iMatchStats_MoneySaved[ MAX_MATCH_STATS_ROUNDS ];			//Saved money, per round
	int m_iMatchStats_KillReward[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_LiveTime[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_Deaths[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_Assists[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_HeadShotKills[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_Objective[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_CashEarned[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_UtilityDamage[ MAX_MATCH_STATS_ROUNDS ];
	int m_iMatchStats_EnemiesFlashed[ MAX_MATCH_STATS_ROUNDS ];

	CUtlReference<CNewParticleEffect> m_ARScreenGlowEffect;

private:
	void AnimateGlows( void );
	void UpdateGlows( void );
	void UpdateFlashBangEffect( void );

	CountdownTimer m_ladderSurpressionTimer;
	Vector m_lastLadderNormal;
	Vector m_lastLadderPos;

	ClientCCHandle_t	m_CCDeathHandle;	// handle to death cc effect
	float				m_flDeathCCWeight;	// for fading in cc effect	

	bool	m_bFreezeFrameCloseOnKiller;
	int		m_nFreezeFrameShiftSideDist;
	Vector	m_vecFreezeFrameEnd;
	QAngle	m_vecFreezeFrameAnglesStart;
	float	m_flFreezeFrameTilt;	

	bool	m_bOldIsScoped;
	
	CNetworkVar( QuestProgress::Reason, m_nQuestProgressReason );

	CNetworkVar( uint16, m_unCurrentEquipmentValue );
	CNetworkVar( uint16, m_unRoundStartEquipmentValue );
	CNetworkVar( uint16, m_unFreezetimeEndEquipmentValue );

	CNetworkArray( MedalRank_t, m_rank, MEDAL_CATEGORY_COUNT );

	CNetworkVar( uint16, m_unMusicID );

	void ProcessSpottedEntityUpdate( void );
	void UpdateSoundEvents( void );
//	void HandleTauntSound( void );

	void CreateNewCustomClothingModels( void );
	void UpdateCustomClothingModels( void );

private:
	
	bool ApplyCompositeMaterialsToCustomClothingModel( C_BaseAnimating* pTargetEntity, CEconItemView *pItem, int nSlotId, bool bForceRegenerate /*= false */ ) const;

	void CreateAddonModel( int i );
	void UpdateAddonModels( bool bForce = false );
	bool m_bAddonModelsAreOutOfDate;
	void UpdateHostageCarryModels();

	void PushawayThink();

	Vector m_vecThirdPersonViewPositionOverride;
	virtual Vector GetThirdPersonViewPosition( void );

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] Added for auto-buy functionality
	//=============================================================================
	void FireGameEvent( IGameEvent *event );
	//=============================================================================
	// HPE_END
	//=============================================================================


	int		m_iAccount;	
	bool	m_bHasHelmet;
	bool	m_bHasHeavyArmor;
	int		m_iClass;
	int		m_ArmorValue;
	QAngle	m_angEyeAngles;
	bool	m_bHasDefuser;
	bool	m_bInHostageRescueZone;
	float	m_fNextThinkPushAway;

	// Glow stuff
	float	m_fNextGlowCheckUpdate;
	float	m_fNextGlowCheckInterval;
	float	m_fGlowAlpha;
	float	m_fGlowAlphaTarget;
	float	m_fGlowAlphaUpdateTime;
	float	m_fGlowAlphaTargetTime;

	static bool s_bPlayingFreezeCamSound;
	bool	m_bShouldAutobuyDMWeapons;

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] Added for auto-buy functionality
	//=============================================================================
	bool    m_bShouldAutobuyNow;
	//=============================================================================
	// HPE_END
	//=============================================================================

#ifdef CS_SHIELD_ENABLED
	bool	m_bHasShield;
	bool	m_bShieldDrawn;
#endif

	bool	m_bHud_MiniScoreHidden;
	bool	m_bHud_RadarHidden;

	int m_nLastKillerIndex;
	int m_nLastConcurrentKilled;
	int m_nDeathCamMusic;

	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;

	// ID Target
	int	m_iIDEntIndex;	
	CountdownTimer		m_delayTargetIDTimer;
	
	int					m_iTargetedWeaponEntIndex;

	// Show the ID target after the cursor leaves the entity
	int					m_iOldIDEntIndex;
	CountdownTimer		m_holdTargetIDTimer;	

	void ReleaseFlashlight( void );
	Beam_t	*m_pFlashlightBeam;

	class CCSSoundEvent
	{
	public:
		CCSSoundEvent() : m_bHasSoundOrigin(false) {}
		string_t m_SoundName;
		float m_flEventTime;	// Play the event when gpGlobals->curtime goes past this.
		Vector m_SoundOrigin;
		bool m_bHasSoundOrigin;
	};
	CUtlLinkedList<CCSSoundEvent,int> m_SoundEvents;

	// List of sounds to play at start of round
	CStartOfRoundAudioPlayback	m_StartOfRoundSoundEvents;

	// manage per play music
	CsMusicType_t m_nCurrentMusic;
	float m_flCurrentMusicStartTime;
	float m_flMusicRoundStartTime;
	
	// This is the list of addons hanging off the guy (grenades, C4, nightvision, etc).
	CUtlLinkedList<CAddonModel, int> m_AddonModels;
	int m_iLastAddonBits;
	int m_iLastPrimaryAddon;
	int m_iLastSecondaryAddon;

	CUtlReference<CNewParticleEffect> m_hC4AddonLED;
	CUtlReference<CNewParticleEffect> m_hC4WeaponLED;

	int m_cycleLatch;				// server periodically updates this to fix up our anims, here it is a 4 bit fixed point
	float m_serverIntendedCycle;	// server periodically updates this to fix up our anims, here it is the float we want, or -1 for no override

	// [tj] Network variables that track who are dominating and being dominated by
	CNetworkArray( bool, m_bPlayerDominated, MAX_PLAYERS+1 );		// array of state per other player whether player is dominating other players
	CNetworkArray( bool, m_bPlayerDominatingMe, MAX_PLAYERS+1 );	// array of state per other player whether other players are dominating this player

	CNetworkArray( int, m_iWeaponPurchasesThisRound, MAX_WEAPONS );		// number of times each weapon purchased this round; used to limit repurchases

	CNetworkVar( bool, m_bIsTaunting );
	CNetworkVar( bool, m_bIsThirdPersonTaunt );
	CNetworkVar( bool, m_bIsHoldingTaunt );
	CNetworkVar( float, m_flTauntYaw );

	CNetworkVar( bool, m_bIsLookingAtWeapon );
	CNetworkVar( bool, m_bIsHoldingLookAtWeapon );

	bool				m_bWasTaunting;
	bool				m_bTauntInterpolating;
	CameraThirdData_t	m_TauntCameraData;
	float				m_flTauntCamCurrentDist;
	float				m_flTauntCamTargetDist;
	QAngle				m_angTauntPredViewAngles;
	QAngle				m_angTauntEngViewAngles;

public:
	float m_flThirdpersonRecoil;

	// taser items
	float m_nextTaserShakeTime;
	float m_firstTaserShakeTime;

	float m_flLastSmokeOverlayAlpha;
	
	C_CSPlayer( const C_CSPlayer & );

	// For interpolating between observer targets 
protected:
	void						UpdateObserverTargetVisibility( void ) const;
	Vector						m_vecObserverInterpolateOffset;			// Offset vec applied to the view which decays over time
	Vector						m_vecObserverInterpStartPos;
	float						m_flObsInterp_PathLength;				// Full path lenght being interpolated
	Quaternion					m_qObsInterp_OrientationStart;
	Quaternion					m_qObsInterp_OrientationTravelDir;
	eObserverInterpState		m_obsInterpState;
	bool						m_bObserverInterpolationNeedsDeferredSetup;

public:
	bool								ShouldInterpolateObserverChanges() const;
	void								StartObserverInterpolation( const QAngle &startAngles );
	virtual eObserverInterpState		GetObserverInterpState( void ) const OVERRIDE;
	virtual bool						IsInObserverInterpolation( void ) const OVERRIDE { return ShouldInterpolateObserverChanges() && m_obsInterpState != OBSERVER_INTERP_NONE; }
	virtual void						SetObserverTarget( EHANDLE hObserverTarget ) OVERRIDE;
	void								InterpolateObserverView( Vector& vOrigin, QAngle& vAngles );
	Vector								GetObserverInterpolatedOffsetVector( void ) { return m_vecObserverInterpolateOffset; }

	bool IsAssassinationTarget( void ) const;

private:
	CNetworkVar( bool, m_bIsAssassinationTarget );	// This player is an assassination target for an active mission

#if CS_CONTROLLABLE_BOTS_ENABLED

public: 
	bool IsControllingBot()							const { return m_bIsControllingBot; }
	bool CanControlObservedBot()					const { return m_bCanControlObservedBot; }

	int GetControlledBotIndex()						const { return m_iControlledBotEntIndex; }

	virtual bool	GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool	GetAttachment( int number, Vector &origin );
	virtual	bool	GetAttachment( int number, Vector &origin, QAngle &angles );

private:
	bool		m_bIsControllingBot;	// Are we controlling a bot? 
	// Note that this can be TRUE even if GetControlledPlayer() returns NULL, IFF we started controlling a bot and then the bot was deleted for some reason. 
	bool		m_bCanControlObservedBot;	// True if the player can control the bot s/he is observing
	int			m_iControlledBotEntIndex;


	void	UpdateFirstPersonShadowCasters( void );

	float		m_flNextMagDropTime;
	int			m_nLastMagDropAttachmentIndex;
	CNetworkVar( bool, m_bHasControlledBotThisRound );

#endif

public: 
	int GetEloBracket( void ) const;
	void SetEloBracket ( int bracket );

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

public:
	// IHasAttributes
	CAttributeManager		*GetAttributeManager( void ) {
#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
		return &m_AttributeManager;
#else
		return NULL;
#endif
	}
	CAttributeContainer		*GetAttributeContainer( void ) { return NULL; }
	CBaseEntity				*GetAttributeOwner( void ) { return NULL; }
	CAttributeList			*GetAttributeList( void ) {
#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
		return &m_AttributeList;
#else
		return NULL;
#endif
	}
	virtual void			ReapplyProvision( void ) { return; }

#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
protected:
	CNetworkVarEmbedded(	CAttributeContainerPlayer, m_AttributeManager );
#endif

	//----------------------------
	// INVENTORY MANAGEMENT
public:
	// Inventory access
#if defined ENABLE_CLIENT_INVENTORIES_FOR_OTHER_PLAYERS
	CCSPlayerInventory	*Inventory( void );
	CEconItemView *GetEquippedItemInLoadoutSlot( int iLoadoutSlot ) { return Inventory()->GetInventoryItemByItemID( m_EquippedLoadoutItemIndices[iLoadoutSlot] ); }
	CEconItemView *GetEquippedItemInLoadoutSlotOrBaseItem( int iLoadoutSlot );
#endif

private:
	// Items that have been equipped on this player instance (the inventory loadout may have changed)
	itemid_t				m_EquippedLoadoutItemIndices[LOADOUT_POSITION_COUNT];

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

public:
	void	ClearAllBulletHitModels( void );
	void	UpdateAllBulletHitModels( void );
	CUtlVector< C_BulletHitModel* > m_vecBulletHitModels;

	// This code allowed us to measure discrepency between client and server bullet hits.
	// It became obsolete when we started using a separate seed for client and server
	// to eliminate 'rage' hacks.
	//
 	CUtlVector< clientHitVerify_t > m_vecBulletVerifyListClient;
 	CUtlVector< clientHitVerify_t > m_vecBulletVerifyListServer;
 
 	void	CompareClientServerBulletHits( void );
 
 	CUserMessageBinder m_UMCMsgReportHit;
 
 	uint8 m_ui8ClientServerHitDifference;

	// Ammo count at death OGS data
	struct roundEndAmmoCount_t
	{
		roundEndAmmoCount_t()
		{
			V_memset( this, 0, sizeof( *this ) );
		}

		int		nPrimaryWeaponDefIndex;
		int		nPrimaryWeaponAmmoCount;

		int		nSecondaryWeaponDefIndex;
		int		nSecondaryWeaponAmmoCount;
	
	};

	roundEndAmmoCount_t m_roundEndAmmoCount;

	Vector m_vecLastAliveLocalVelocity;

private:
	void RecordAmmoForRound( void );

	Vector m_vecLastContactShadowTraceOriginLeft;
	Vector m_vecLastContactShadowTraceOriginRight;
	float m_flLastContactShadowGroundHeightLeft;
	float m_flLastContactShadowGroundHeightRight;

public:

	EHANDLE m_hContactShadowLeft;
	EHANDLE m_hContactShadowRight;

	void CreateClientEffectModels( void );
	void RemoveClientEffectModels( void );

};

C_CSPlayer* GetLocalOrInEyeCSPlayer( void );
C_CSPlayer* GetHudPlayer( void );	// get the player we should show the HUD for (local or observed)

inline C_CSPlayer *ToCSPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return assert_cast<C_CSPlayer*>( pEntity );
}

namespace vgui
{
	class IImage;
}
vgui::IImage* GetDefaultAvatarImage( C_BasePlayer *pPlayer );


void AddSmokeGrenade( Vector location, int iEntityId );
void AddSmokeGrenadeHandle( EHANDLE hGrenade );
void RemoveSmokeGrenade( Vector location, int iEntityId );
void RemoveSmokeGrenadeHandle( EHANDLE hGrenade );
void RemoveAllSmokeGrenades( void );
void RemoveSmokeGrenadeHandle( EHANDLE hGrenade );
bool LineGoesThroughSmoke( Vector from, Vector to, bool grenadeBloat );
void TruncatePlayerName( wchar_t *pCleanedHTMLName, int destLen, int nTruncateAt, bool bSkipHTML = false );

class C_CSRagdoll : public C_BaseAnimatingOverlay
{
public:
	DECLARE_CLASS( C_CSRagdoll, C_BaseAnimatingOverlay );
	DECLARE_CLIENTCLASS();
	
	C_CSRagdoll();
	~C_CSRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;
	void GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );
	void GetRagdollInitBoneArraysYawMode( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt );

	void ApplyRandomTaserForce( void );
	void ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName );
	bool IsInitialized() { return m_bInitialized; }
	virtual void ValidateModelIndex( void );

	void SetRagdollClientSideAddon( uint32 uiAddonMask );
	
	int	BloodColor(){ return BLOOD_COLOR_RED; }

	void ApplySemiRandomDirectionalForce( Vector vecDir, float flStrength );

	void AttachWearableGibsFromPlayer( C_CSPlayer *pParentPlayer );
	void DestroyAttachedWearableGibs( void );
	
	int GetDeathPose( void ) { return m_iDeathPose; }

private:
	
	C_CSRagdoll( const C_CSRagdoll & ) :m_nGlowObjectHandle( -1) { }
	void DestroyGlowObject();

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );

	void CreateLowViolenceRagdoll( void );
	void CreateCSRagdoll( void );

private:

	EHANDLE	m_hPlayer;
	CHandle<C_BaseAnimating> m_hHolidayHatAddon;
	CHandle<C_BaseAnimating> m_hHolidayGhostAddon;
	CHandle<C_BaseAnimating> m_hAssassinationTargetAddon;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	CNetworkVar(int, m_iDeathPose );
	CNetworkVar(int, m_iDeathFrame );
	CNetworkVar(float, m_flDeathYaw );
	CNetworkVar(float, m_flAbsYaw );
	bool m_bInitialized;

	int m_nGlowObjectHandle;
};


#endif // C_CS_PLAYER_H
