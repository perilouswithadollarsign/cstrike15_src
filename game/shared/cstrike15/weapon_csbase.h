//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_CSBASE_H
#define WEAPON_CSBASE_H
#ifdef _WIN32
#pragma once
#endif

#if !defined (_GAMECONSOLE)
	#include "econ_item_view.h"
#endif

#include "cs_playeranimstate.h"
#include "cs_weapon_parse.h"
#include "cs_shareddefs.h"
#ifdef CLIENT_DLL
#include "glow_outline_effect.h"
#endif

#ifdef IRONSIGHT
#include "weapon_ironsightcontroller.h"
#endif //IRONSIGHT

#if defined( CLIENT_DLL )
	#define CWeaponCSBase C_WeaponCSBase
#endif

// extern int  ClassnameToWeaponID( const char *classname );
extern CSWeaponID AliasToWeaponID( const char *alias );
extern const char *WeaponIDToAlias( int id );
extern const char *GetTranslatedWeaponAlias( const char *alias);
extern const char * GetWeaponAliasFromTranslated(const char *translatedAlias);
extern bool	IsPrimaryWeapon( CSWeaponID id );
extern bool IsSecondaryWeapon( CSWeaponID  id );
extern bool IsGrenadeWeapon( CSWeaponID id );
extern bool IsArmor( CSWeaponID id );
extern int GetShellForAmmoType( const char *ammoname );

#define SHIELD_VIEW_MODEL "models/weapons/v_shield.mdl"
#define SHIELD_WORLD_MODEL "models/weapons/w_shield.mdl"

class CCSPlayer;

// These are the names of the ammo types that go in the CAmmoDefs and that the 
// weapon script files reference.
#define BULLET_PLAYER_50AE		"BULLET_PLAYER_50AE"
#define BULLET_PLAYER_762MM		"BULLET_PLAYER_762MM"
#define BULLET_PLAYER_556MM		"BULLET_PLAYER_556MM"
#define BULLET_PLAYER_556MM_SMALL "BULLET_PLAYER_556MM_SMALL"
#define BULLET_PLAYER_556MM_BOX	"BULLET_PLAYER_556MM_BOX"
#define BULLET_PLAYER_338MAG	"BULLET_PLAYER_338MAG"
#define BULLET_PLAYER_9MM		"BULLET_PLAYER_9MM"
#define BULLET_PLAYER_BUCKSHOT	"BULLET_PLAYER_BUCKSHOT"
#define BULLET_PLAYER_45ACP		"BULLET_PLAYER_45ACP"
#define BULLET_PLAYER_357SIG	"BULLET_PLAYER_357SIG"
#define BULLET_PLAYER_357SIG_P250	"BULLET_PLAYER_357SIG_P250"
#define BULLET_PLAYER_357SIG_SMALL	"BULLET_PLAYER_357SIG_SMALL"
#define BULLET_PLAYER_357SIG_MIN	"BULLET_PLAYER_357SIG_MIN"
#define BULLET_PLAYER_57MM		"BULLET_PLAYER_57MM"
#define AMMO_TYPE_HEGRENADE		"AMMO_TYPE_HEGRENADE"
#define AMMO_TYPE_FLASHBANG		"AMMO_TYPE_FLASHBANG"
#define AMMO_TYPE_SMOKEGRENADE	"AMMO_TYPE_SMOKEGRENADE"
#define AMMO_TYPE_DECOY			"AMMO_TYPE_DECOY"
#define AMMO_TYPE_MOLOTOV		"AMMO_TYPE_MOLOTOV"
#define AMMO_TYPE_TASERCHARGE	"AMMO_TYPE_TASERCHARGE"
#define AMMO_TYPE_HEALTHSHOT	"AMMO_TYPE_HEALTHSHOT"
#define AMMO_TYPE_TAGRENADE	"AMMO_TYPE_TAGRENADE"

#define CROSSHAIR_CONTRACT_PIXELS_PER_SECOND	7.0f

// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// MIKETODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );

enum CSWeaponMode
{
	Primary_Mode = 0,
	Secondary_Mode,
	WeaponMode_MAX
};

// structure to encapsulate state of head bob
struct BobState_t
{
	BobState_t() 
	{ 
		m_flBobTime = 0; 
		m_flLastBobTime = 0;
		m_flLastSpeed = 0;
		m_flVerticalBob = 0;
		m_flLateralBob = 0;
		m_flRawVerticalBob = 0;
		m_flRawLateralBob = 0;
	}

	float m_flBobTime;
	float m_flLastBobTime;
	float m_flLastSpeed;
	float m_flVerticalBob;
	float m_flLateralBob;
	float m_flRawVerticalBob;
	float m_flRawLateralBob;
};

#ifdef CLIENT_DLL
float CalcViewModelBobHelper( CBasePlayer *player, BobState_t *pBobState, int nVMIndex = 0 );
void AddViewModelBobHelper( Vector &origin, QAngle &angles, BobState_t *pBobState );

class CCSWeaponVisualsDataProcessor;
#endif

class CWeaponCSBase : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponCSBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponCSBase();
	virtual ~CWeaponCSBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();

		virtual void CheckRespawn();
		virtual CBaseEntity* Respawn();
		
		virtual const Vector& GetBulletSpread();
		virtual float	GetDefaultAnimSpeed();

		virtual void	BulletWasFired( const Vector &vecStart, const Vector &vecEnd );
		virtual bool	ShouldRemoveOnRoundRestart();
		virtual void    OnRoundRestart();

		virtual bool	DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

		void SendActivityEvents( int nActEvents = ACT_VM_RELOAD );

		void Materialize();
		void AttemptToMaterialize();
		virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

		virtual bool IsRemoveable();

		virtual void RemoveUnownedWeaponThink();
		
	#endif

	virtual int GetShotgunReloadState( void ) { return 0; } // only shotguns use this for multi-stage reloads

	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	BobState_t		*GetBobState();
	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const;

 	bool			IsPistol() const;


	virtual int		GetWeaponPrice() const { return GetCSWpnData().										GetWeaponPrice( GetEconItemView() ); }
	virtual bool	IsFullAuto() const { return GetCSWpnData().											IsFullAuto( GetEconItemView() ); }
	virtual int		GetDamage() const { return GetCSWpnData().											GetDamage( GetEconItemView() ); }
	virtual int		GetKillAward() const { return GetCSWpnData().										GetKillAward( GetEconItemView() ); }
	virtual float	GetCycleTime( int mode = Primary_Mode ) const { return GetCSWpnData().				GetCycleTime( GetEconItemView(), mode ); }
	virtual float	GetArmorRatio() const { return GetCSWpnData().										GetArmorRatio( GetEconItemView() ); }
	virtual bool	HasTraditionalScope() const { return GetCSWpnData().								HasTraditionalScope( GetEconItemView() ); }
	virtual float	GetInaccuracyStand( int mode = Primary_Mode ) const { return GetCSWpnData().		GetInaccuracyStand( GetEconItemView(), mode ); }
	virtual float	GetInaccuracyCrouch( int mode = Primary_Mode ) const { return GetCSWpnData().		GetInaccuracyCrouch( GetEconItemView(), mode ); }

	virtual bool	CannotShootUnderwater() const { return GetCSWpnData().								CannotShootUnderwater( GetEconItemView() ); }

	virtual int		GetRecoilMagnitude( int mode ) const { return GetCSWpnData().						GetRecoilMagnitude( GetEconItemView(), mode ); }
	virtual int		GetRecoilMagnitudeVariance( int mode ) const { return GetCSWpnData().				GetRecoilMagnitudeVariance( GetEconItemView(), mode ); }
	virtual int		GetRecoilAngle( int mode ) const { return GetCSWpnData().							GetRecoilAngle( GetEconItemView(), mode ); }
	virtual int		GetRecoilAngleVariance( int mode ) const { return GetCSWpnData().					GetRecoilAngleVariance( GetEconItemView(), mode ); }

	virtual float	GetMaxSpeed() const { return GetCSWpnData().										GetMaxSpeed( GetEconItemView(), m_weaponMode ); }	// What's the player's max speed while holding this weapon.

	virtual int		GetZoomLevels() const { return GetCSWpnData().										GetZoomLevels( GetEconItemView() ); }

	virtual bool		CanBeUsedWithShield() const	{ return GetCSWpnData().							CanBeUsedWithShield(); }
	virtual const char*	GetZoomInSound() const	{ return GetCSWpnData().								GetZoomInSound(); }
	virtual const char*	GetZoomOutSound() const	{ return GetCSWpnData().								GetZoomOutSound(); }
	virtual float		GetBotAudibleRange() const { return GetCSWpnData().								GetBotAudibleRange(); }
	virtual const char* GetWrongTeamMsg() const { return GetCSWpnData().								GetWrongTeamMsg(); }
	virtual const char* GetAnimExtension() const { return GetCSWpnData().								GetAnimExtension(); }
	virtual const char* GetShieldViewModel() const { return GetCSWpnData().								GetShieldViewModel(); }
	virtual const char* GetSilencerModel() const { return GetCSWpnData().								GetSilencerModel(); }
	virtual float		GetAddonScale() const { return GetCSWpnData().									GetAddonScale(); }
	virtual float		GetThrowVelocity() const { return GetCSWpnData().								GetThrowVelocity(); }

	virtual int		GetZoomFOV( int nZoomLevel ) const;
	virtual float	GetZoomTime( int nZoomLevel ) const;


	virtual CSWeaponType GetWeaponType( void ) const { return GetCSWpnData().GetWeaponType( GetEconItemView() ); }
	virtual const char	*GetDefinitionName( void ) const { return GetEconItemView()->GetStaticData()->GetDefinitionName(); }

	CCSPlayer* GetPlayerOwner() const;
#ifdef CLIENT_DLL
	C_BaseEntity *GetWeaponForEffect();
	void UpdateOutlineGlow( void );
	CGlowObject m_GlowObject;
#endif


	virtual int GetRecoilSeed( void ) const;

	// Get CS-specific weapon data.
	virtual CCSWeaponInfo const	&GetCSWpnData() const;
	virtual int GetCSZoomLevel() { return 0; }

	// Get specific CS weapon ID (ie: WEAPON_AK47, etc)
	virtual CSWeaponID GetCSWeaponID( void ) const { return m_nWeaponID; }

	// return true if this weapon is an instance of the given weapon type (ie: "IsA" WEAPON_GLOCK)
	bool IsA( CSWeaponID id ) const						{ return GetCSWeaponID() == id; }

	// return true if this weapon is a kinf of the given weapon type (ie: "IsKindOf" WEAPONTYPE_RIFLE )
	bool IsKindOf( CSWeaponType type ) const			{ return GetCSWpnData().GetWeaponType() == type; }

	const char		*GetTracerType( void ) { return GetCSWpnData().GetTracerEffectName( GetEconItemView() ); }

	loadout_positions_t GetDefaultLoadoutSlot( void ) { return (loadout_positions_t)( GetEconItemView() ? GetEconItemView()->GetItemDefinition()->GetLoadoutSlot( 0 ) : 0 ); }

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const				{ return m_bSilencerOn; }
	// return true is this weapon is capable of being silenced
	// TODO: allow weapons to define this in the weapon script file and have it live in WpnData
	virtual bool HasSilencer( void ) const				
	{ 
		return GetCSWpnData().HasSilencer( GetEconItemView() ); 
	}

	virtual void SetWeaponModelIndex( const char *pName );
	virtual void OnPickedUp( CBaseCombatCharacter *pNewOwner );

	virtual void OnJump( float fImpulse );
	virtual void OnLand( float fVelocity );

	float GetRecoveryTime( void );

	virtual bool	HasZoom() { return false; }
	void			CallSecondaryAttack();

public:
	#if defined( CLIENT_DLL )

		virtual void	ProcessMuzzleFlashEvent();
		virtual bool	OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
		virtual bool	ShouldPredict();
		virtual void	DrawCrosshair();
		virtual void	OnDataChanged( DataUpdateType_t type );

		virtual int		GetMuzzleAttachmentIndex_1stPerson( C_BaseViewModel *pViewModel );
		virtual int		GetMuzzleAttachmentIndex_3rdPerson( void );
		virtual int		GetEjectBrassAttachmentIndex_1stPerson( C_BaseViewModel *pViewModel );
		virtual int		GetEjectBrassAttachmentIndex_3rdPerson( void );

		virtual bool	DoesHideViewModelWhenZoomed( void ) { return GetCSWpnData().					DoesHideViewModelWhenZoomed( GetEconItemView() ); }

		float			m_flCrosshairDistance;
		int				m_iAmmoLastCheck;
		int				m_iAlpha;
		int				m_iScopeTextureID;
		int				m_iCrosshairTextureID; // for white additive texture
		float			m_flGunAccuracyPosition;

		virtual const char* GetMuzzleFlashEffectName_1stPerson( void );
		virtual const char* GetMuzzleFlashEffectName_3rdPerson( void );
		virtual const char* GetEjectBrassEffectName( void );
		virtual const char* GetHeatEffectName( void );

		int GetReticleWeaponSpread( void );
		int GetReticleCrosshairGap( void );
		bool WantReticleShown( void );

	#else
		virtual void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
		virtual bool	Reload();
		virtual void	Spawn();
		virtual bool	KeyValue( const char *szKeyName, const char *szValue );

		virtual bool PhysicsSplash( const Vector &centerPoint, const Vector &normal, float rawSpeed, float scaledSpeed );

	#endif

	bool IsUseable();
	virtual bool	CanDeploy( void );
	virtual void	UpdateShieldState( void );
	virtual bool	SendWeaponAnim( int iActivity );
	virtual void	SendViewModelAnim( int nSequence );
	virtual void	SecondaryAttack( void );
	virtual bool	CanPrimaryAttack( void ) {return true;}
	virtual void	Precache( void );
	virtual bool	CanBeSelected( void );
	virtual Activity GetDeployActivity( void );
	virtual bool	DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt );
	virtual void 	DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	virtual bool	DefaultPistolReload();

	virtual bool	Deploy();
	virtual void	Drop( const Vector &vecVelocity );
	bool PlayEmptySound();
	virtual const char *GetShootSound( int iIndex ) const;
	virtual const char *GetPlayerAnimationExtension( void ) const;
	virtual const char *GetAddonModel( void ) const;
	virtual void	ItemPostFrame();
	virtual void	ItemBusyFrame();
	virtual const char *GetViewModel( int viewmodelindex = 0 ) const;
	virtual void WeaponReset( void ) {}
	virtual bool	WeaponHasBurst() const { return false; }
	virtual bool	IsInBurstMode() { return m_bBurstMode; }
	virtual bool	IsRevolver() const { return false; }

	void			ItemPostFrame_ProcessPrimaryAttack( CCSPlayer *pPlayer );
	bool			ItemPostFrame_ProcessZoomAction( CCSPlayer *pPlayer );
	bool			ItemPostFrame_ProcessSecondaryAttack( CCSPlayer *pPlayer );
	void			ItemPostFrame_ProcessReloadAction( CCSPlayer *pPlayer );
	void			ItemPostFrame_ProcessIdleNoAction( CCSPlayer *pPlayer );

	void			ItemPostFrame_RevolverResetHaulback();

	CNetworkVar( CSWeaponMode, m_weaponMode);

	virtual float GetInaccuracy() const;
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	virtual float GetAccuracyFishtail() const { return m_fAccuracyFishtail; }
	virtual void SetAccuracyFishtail( float fFishtail ) { m_fAccuracyFishtail = fFishtail; }
#endif
	virtual float GetSpread() const { return GetCSWpnData().GetSpread( GetEconItemView(), m_weaponMode.Get() ); }

	virtual void UpdateAccuracyPenalty();

	CNetworkVar( float, m_fAccuracyPenalty );
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	CNetworkVar( float, m_fAccuracyFishtail );
#endif
	float m_fAccuracySmoothedForZoom;
	float m_fScopeZoomEndTime;
	CNetworkVar( int, m_iRecoilIndex );	// DEPRECATED. Kept for old demo compatibility.
	CNetworkVar( float, m_flRecoilIndex );
	CNetworkVar( bool, m_bBurstMode );

	CNetworkVar( float, m_flPostponeFireReadyTime );
	void ResetPostponeFireReadyTime( void ) { m_flPostponeFireReadyTime = FLT_MAX; }
	void SetPostponeFireReadyTime( float flFutureTime ) { m_flPostponeFireReadyTime = flFutureTime; }
	bool IsPostponFireReadyTimeElapsed( void ) { return (m_flPostponeFireReadyTime < gpGlobals->curtime); }

	virtual bool IsReloadVisuallyComplete() {return m_bReloadVisuallyComplete; }
	CNetworkVar( bool, m_bReloadVisuallyComplete );

	CNetworkVar( bool, m_bSilencerOn );
	CNetworkVar( float, m_flDoneSwitchingSilencer );	// soonest time switching the silencer will be complete
	CNetworkVar( float, m_flDroppedAtTime ); // when was this weapon last dropped

	bool IsSwitchingSilencer( void ) { return ( m_flDoneSwitchingSilencer >= gpGlobals->curtime ); }

	void SetExtraAmmoCount( int count ) { m_iExtraPrimaryAmmo = count; }
	int GetExtraAmmoCount( void ) const { return m_iExtraPrimaryAmmo; }

	
	void SetPreviousOwner( CCSPlayer* player ) { m_hPrevOwner = ( CBasePlayer * )player; }
	CCSPlayer* GetPreviousOwner() const { return ( CCSPlayer* )m_hPrevOwner.Get(); }

    // [tj] Accessors for the donor system
	void SetDonor( CCSPlayer* player ) { m_donor.Set( player ); }
    CCSPlayer* GetDonor() const { return m_donor.Get(); }
    void SetDonated(bool donated) { m_donated = true;}
    bool GetDonated() const { return m_donated; }

    //[dwenger] Accessors for the prior owner list
	void AddToPriorOwnerList( CCSPlayer* pPlayer );
	bool IsAPriorOwner( CCSPlayer* pPlayer ) const;

	CCSPlayer * GetOriginalOwner();

	CNetworkVar( int, m_iOriginalTeamNumber );

	int GetOriginalTeamNumber()	{ return m_iOriginalTeamNumber; }

	void AddPriorOwner( CCSPlayer* pPlayer );
	bool WasOwnedByTeam( int teamNumber );
	bool CanBePickedUp( void ) { return m_bCanBePickedUp; }

#ifdef CLIENT_DLL
	virtual void SaveCustomMaterialsTextures( void );
#endif

protected:

	float CalculateNextAttackTime( float flCycleTime );
	void Recoil( CSWeaponMode weaponMode );

	bool m_bCanBePickedUp;

private:

	CWeaponCSBase( const CWeaponCSBase & );

	virtual int GetWeaponID( void ) const		{ return GetCSWeaponID(); }
#ifdef CLIENT_DLL
	void UpdateCustomMaterial( void );
	void CheckCustomMaterial( void );
	bool m_bVisualsDataSet;
	bool m_bOldFirstPersonSpectatedState;
#endif

	CSWeaponID	m_nWeaponID;

	int		m_iExtraPrimaryAmmo;
	
	float	m_nextOwnerTouchTime;
	float	m_nextPrevOwnerTouchTime;

	CNetworkHandle( CBasePlayer, m_hPrevOwner );

	int m_iDefaultExtraAmmo;

	// [dwenger] track all prior owners of this weapon
	CUtlVector< CCSPlayer* > m_PriorOwners;

	// [tj] To keep track of people who drop weapons for teammates during the buy round
	CHandle<CCSPlayer> m_donor;
	bool m_donated;

	void ResetGunHeat( void );
	void UpdateGunHeat( float heat, int iAttachmentIndex );

	CNetworkVar( float, m_fLastShotTime );

	bool m_bWasOwnedByCT;
	bool m_bWasOwnedByTerrorist;

#ifdef CLIENT_DLL
	
	int  GetViewModelSplitScreenSlot( C_BaseViewModel *pViewModel );

	// Smoke effect variables.
	float m_gunHeat;
	unsigned int m_smokeAttachments;
	float m_lastSmokeTime;

#else

	bool m_bFiredOutOfAmmoEvent;
	int m_numRemoveUnownedWeaponThink;

#endif

public:

#ifdef CLIENT_DLL
	float m_flLastClientFireBulletTime;
#endif

#ifdef IRONSIGHT
	CIronSightController *GetIronSightController( void );
	void				 UpdateIronSightController( void );
	CIronSightController *m_IronSightController;
	CNetworkVar( int, m_iIronSightMode );
#endif //IRONSIGHT


};


extern ConVar weapon_recoil_decay1;
extern ConVar weapon_recoil_decay2_exp;
extern ConVar weapon_recoil_decay2_lin;
extern ConVar weapon_recoil_vel_decay;
extern ConVar weapon_recoil_extra;
extern ConVar weapon_recoil_scale;
extern ConVar weapon_recoil_scale_motion_controller;


#endif // WEAPON_CSBASE_H
