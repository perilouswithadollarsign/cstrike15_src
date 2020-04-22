//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_PLAYER_H
#define CS_PLAYER_H
#pragma once


#include "basemultiplayerplayer.h"
#include "server_class.h"
#include "cs_gamerules.h"
#include "cs_playeranimstate.h"
#include "cs_shareddefs.h"
#include "cs_autobuy.h"
#include "utldict.h"
#include "usermessages.h"

#include "cstrike15_item_inventory.h"

class CWeaponCSBase;
class CMenu;
class CHintMessageQueue;
class CNavArea;
class CCSBot;
class CEconPersonaDataPublic;
class CCSUsrMsg_PlayerDecalDigitalSignature;

#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "matchmaking/iplayerrankingdata.h"

#include "cs_player_rank_shared.h"

#include "cs_player_shared.h"

#include "csgo_playeranimstate.h"

#define MENU_STRING_BUFFER_SIZE	1024
#define MENU_MSG_TEXTCHUNK_SIZE	50

#define CS_FORCE_TEAM_THINK_CONTEXT	"CSForceTeamThink"

enum
{
	MIN_NAME_CHANGE_INTERVAL = 0,			// minimum number of seconds between name changes
	NAME_CHANGE_HISTORY_SIZE = 5,			// number of times a player can change names in NAME_CHANGE_HISTORY_INTERVAL
	NAME_CHANGE_HISTORY_INTERVAL = 600,		// no more than NAME_CHANGE_HISTORY_SIZE name changes can be made in this many seconds
};


extern ConVar bot_mimic;


// Function table for each player state.
class CCSPlayerStateInfo
{
public:
	CSPlayerState m_iPlayerState;
	const char *m_pStateName;
	
	void (CCSPlayer::*pfnEnterState)();	// Init and deinit the state.
	void (CCSPlayer::*pfnLeaveState)();

	void (CCSPlayer::*pfnPreThink)();	// Do a PreThink() in this state.
};


//=======================================
//Record of either damage taken or given.
//Contains the player name that we hurt or that hurt us,
//and the total damage
//=======================================
class CDamageRecord
{
public:
	CDamageRecord( CCSPlayer * pPlayerDamager, CCSPlayer * pPlayerRecipient, int iDamage, int iCounter, int iActualHealthRemoved );

	void AddDamage( int iDamage, int iCounter, int iActualHealthRemoved = 0 )
	{
		m_iDamage += iDamage;
		m_iActualHealthRemoved += iActualHealthRemoved;

		if ( m_iLastBulletUpdate != iCounter || GetPlayerDamagerPtr() == NULL )
			m_iNumHits++;

		m_iLastBulletUpdate = iCounter;
	}

	bool IsDamageRecordStillValidForDamagerAndRecipient( CCSPlayer * pPlayerDamager, CCSPlayer * pPlayerRecipient );
	bool IsDamageRecordValidPlayerToPlayer(void) { return ( this && m_PlayerDamager && m_PlayerRecipient) ? true : false; }

	CCSPlayer* GetPlayerDamagerPtr( void ) { return m_PlayerDamager; }
	CCSPlayer* GetPlayerRecipientPtr( void ) { return m_PlayerRecipient; }

	char *GetPlayerDamagerName( void ) { return m_szPlayerDamagerName; }
	char *GetPlayerRecipientName( void ) { return m_szPlayerRecipientName; }

	int GetDamage( void ) { return m_iDamage; }
	int GetActualHealthRemoved( void ) { return m_iActualHealthRemoved; }
	int GetNumHits( void ) { return m_iNumHits; }

	CCSPlayer* GetPlayerDamagerControlledBotPtr( void ) { return m_PlayerDamagerControlledBot; }
	CCSPlayer* GetPlayerRecipientControlledBotPtr( void ) { return m_PlayerRecipientControlledBot; }

private:
	CHandle<CCSPlayer> m_PlayerDamager;
	CHandle<CCSPlayer> m_PlayerRecipient;

	char m_szPlayerDamagerName[MAX_PLAYER_NAME_LENGTH];
	char m_szPlayerRecipientName[MAX_PLAYER_NAME_LENGTH];
	
	int m_iDamage;		//how much damage was delivered
	int m_iActualHealthRemoved;		//how much damage was actually applied
	int m_iNumHits;		//how many hits
	int	m_iLastBulletUpdate; // update counter

	CHandle<CCSPlayer> m_PlayerDamagerControlledBot;
	CHandle<CCSPlayer> m_PlayerRecipientControlledBot;
};

// Message display history (CCSPlayer::m_iDisplayHistoryBits)
// These bits are set when hint messages are displayed, and cleared at
// different times, according to the DHM_xxx bitmasks that follow

#define DHF_ROUND_STARTED		( 1 << 1 )
#define DHF_HOSTAGE_SEEN_FAR	( 1 << 2 )
#define DHF_HOSTAGE_SEEN_NEAR	( 1 << 3 )
#define DHF_HOSTAGE_USED		( 1 << 4 )
#define DHF_HOSTAGE_INJURED		( 1 << 5 )
#define DHF_HOSTAGE_KILLED		( 1 << 6 )
#define DHF_FRIEND_SEEN			( 1 << 7 )
#define DHF_ENEMY_SEEN			( 1 << 8 )
#define DHF_FRIEND_INJURED		( 1 << 9 )
#define DHF_FRIEND_KILLED		( 1 << 10 )
#define DHF_ENEMY_KILLED		( 1 << 11 )
#define DHF_BOMB_RETRIEVED		( 1 << 12 )
#define DHF_AMMO_EXHAUSTED		( 1 << 15 )
#define DHF_IN_TARGET_ZONE		( 1 << 16 )
#define DHF_IN_RESCUE_ZONE		( 1 << 17 )
#define DHF_IN_ESCAPE_ZONE		( 1 << 18 ) // unimplemented
#define DHF_IN_VIPSAFETY_ZONE	( 1 << 19 ) // unimplemented
#define	DHF_NIGHTVISION			( 1 << 20 )
#define	DHF_HOSTAGE_CTMOVE		( 1 << 21 )
#define	DHF_SPEC_DUCK			( 1 << 22 )

// DHF_xxx bits to clear when the round restarts

#define DHM_ROUND_CLEAR ( \
	DHF_ROUND_STARTED | \
	DHF_HOSTAGE_KILLED | \
	DHF_FRIEND_KILLED | \
	DHF_BOMB_RETRIEVED )


// DHF_xxx bits to clear when the player is restored

#define DHM_CONNECT_CLEAR ( \
	DHF_HOSTAGE_SEEN_FAR | \
	DHF_HOSTAGE_SEEN_NEAR | \
	DHF_HOSTAGE_USED | \
	DHF_HOSTAGE_INJURED | \
	DHF_FRIEND_SEEN | \
	DHF_ENEMY_SEEN | \
	DHF_FRIEND_INJURED | \
	DHF_ENEMY_KILLED | \
	DHF_AMMO_EXHAUSTED | \
	DHF_IN_TARGET_ZONE | \
	DHF_IN_RESCUE_ZONE | \
	DHF_IN_ESCAPE_ZONE | \
	DHF_IN_VIPSAFETY_ZONE | \
	DHF_HOSTAGE_CTMOVE | \
	DHF_SPEC_DUCK )

// radio messages (these must be kept in sync with actual radio) -------------------------------------
enum RadioType
{
	RADIO_INVALID = 0,

	RADIO_START_1,							///< radio messages between this and RADIO_START_2 and part of Radio1()

	RADIO_GO_GO_GO,
	RADIO_TEAM_FALL_BACK,
	RADIO_STICK_TOGETHER_TEAM,
	RADIO_HOLD_THIS_POSITION,
	RADIO_FOLLOW_ME,

	RADIO_START_2,							///< radio messages between this and RADIO_START_3 are part of Radio2()

	RADIO_AFFIRMATIVE,
	RADIO_NEGATIVE,
	RADIO_CHEER,
	RADIO_COMPLIMENT,
	RADIO_THANKS,

	RADIO_START_3,							///< radio messages above this are part of Radio3()

	RADIO_ENEMY_SPOTTED,
	RADIO_NEED_BACKUP,
	RADIO_YOU_TAKE_THE_POINT,
	RADIO_SECTOR_CLEAR,
	RADIO_IN_POSITION,

	// not used
	///////////////////////////////////
	RADIO_COVER_ME,
	RADIO_REGROUP_TEAM,
	RADIO_TAKING_FIRE,
	RADIO_REPORT_IN_TEAM,
	RADIO_REPORTING_IN,
	RADIO_GET_OUT_OF_THERE,
	RADIO_ENEMY_DOWN,
	RADIO_STORM_THE_FRONT,

	RADIO_END,

	RADIO_NUM_EVENTS
};

extern const char *RadioEventName[ RADIO_NUM_EVENTS+1 ];

/**
 * Convert name to RadioType
 */
extern RadioType NameToRadioEvent( const char *name );

enum BuyResult_e
{
	BUY_BOUGHT,
	BUY_ALREADY_HAVE,
	BUY_CANT_AFFORD,
	BUY_PLAYER_CANT_BUY,	// not in the buy zone, is the VIP, is past the timelimit, etc
	BUY_NOT_ALLOWED,		// weapon is restricted by VIP mode, team, etc
	BUY_INVALID_ITEM,
};

// [tj] The phases for the "Goose Chase" achievement
enum GooseChaseAchievementStep
{
	GC_NONE,    
	GC_SHOT_DURING_DEFUSE,
	GC_STOPPED_AFTER_GETTING_SHOT
};

// [tj] The phases for the "Defuse Defense" achievement
enum DefuseDefenseAchivementStep
{
	DD_NONE,
	DD_STARTED_DEFUSE,
	DD_KILLED_TERRORIST
};



struct quest_data_t
{
public:
	const char * m_szQuestCalcExpression;
	const char * m_szQuestBonusCalcExpression;
	int m_nQuestID;
	int m_nQuestNormalPoints;
	int m_nQuestBonusPoints;
};


//=============================================================================
// >> CounterStrike player
//=============================================================================
class CCSPlayer : public CBaseMultiplayerPlayer, public ICSPlayerAnimStateHelpers
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
	, public IHasAttributes, public IInventoryUpdateListener
#endif
{
public:
	DECLARE_CLASS( CCSPlayer, CBaseMultiplayerPlayer );
	DECLARE_SERVERCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CCSPlayer();
	~CCSPlayer();

	static CCSPlayer *CreatePlayer( const char *className, edict_t *ed );
	static CCSPlayer* Instance( int iEnt );

	virtual void		Precache();
	virtual void		Spawn();
	virtual void		InitialSpawn( void );
	virtual void		UpdateOnRemove( void );
	virtual void		PostSpawnPointSelection( void );
	
	void SetCSSpawnLocation( Vector position, QAngle angle );
	
	virtual void		ImpulseCommands() OVERRIDE;
	virtual void		CheatImpulseCommands( int iImpulse );
	virtual void		PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper );
	virtual void		PostThink();

	void				SprayPaint( CCSUsrMsg_PlayerDecalDigitalSignature const &msg );

	class ITakeDamageListener
	{
	public:
		ITakeDamageListener();
		~ITakeDamageListener();
		virtual void OnTakeDamageListenerCallback( CCSPlayer *pVictim, CTakeDamageInfo &infoTweakable ) = 0;
	};
	virtual int			OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual int			OnTakeDamage_Alive( const CTakeDamageInfo &info );

	virtual void		Event_Killed( const CTakeDamageInfo &info );
	virtual void		Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	virtual void		TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr );

	void FindMatchingWeaponsForTeamLoadout( const char *pchName, int nTeam, bool bMustBeTeamSpecific, CUtlVector< CEconItemView* > &matchingWeapons );
	virtual CBaseEntity	*GiveNamedItem( const char *pchName, int iSubType = 0, CEconItemView *pScriptItem = NULL, bool bForce = false ) OVERRIDE;

	virtual bool		IsBeingGivenItem() const { return m_bIsBeingGivenItem; }
	
	virtual CBaseEntity *FindUseEntity( void );
	virtual bool		IsUseableEntity( CBaseEntity *pEntity, unsigned int requiredCaps );
	
	virtual void		CreateViewModel( int viewmodelindex = 0 );
	virtual void		ShowViewPortPanel( const char * name, bool bShow = true, KeyValues *data = NULL );

	void HandleOutOfAmmoKnifeKills( CCSPlayer* pAttackerPlayer, CWeaponCSBase* pAttackerWeapon );
	// This passes the event to the client's and server's CPlayerAnimState.
	void DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	// from CBasePlayer
	virtual void		SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	virtual bool		ShouldCheckOcclusion( CBasePlayer *pOtherPlayer ) OVERRIDE;


	virtual	bool		ShouldCollide( int collisionGroup, int contentsMask ) const;

	// from CBasePlayer
	virtual bool		IsValidObserverTarget(CBaseEntity * target);
	virtual CBaseEntity* FindNextObserverTarget( bool bReverse );

	virtual int 		GetNextObserverSearchStartPoint( bool bReverse );

	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence ) OVERRIDE;

// In shared code.
public:

	// ICSPlayerAnimState overrides.
	virtual CWeaponCSBase* CSAnim_GetActiveWeapon();
	virtual bool CSAnim_CanMove();

	virtual float GetPlayerMaxSpeed();
	void CheckForWeaponFiredAchievement();

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

	void GetBulletTypeParameters( 
		int iBulletType, 
		float &fPenetrationPower, 
		float &flPenetrationDistance );

	int GetDefaultCrouchedFOV( void ) const;

	void DisplayPenetrationDebug( Vector vecEnter, Vector vecExit, float flDistance, float flInitialDamage, float flDamageLostImpact, float flTotalLostDamage, short nEnterSurf, short nExitSurf );

	// Returns true if the player is allowed to move.
	bool CanMove() const;
	
	// Returns the player mask which includes the solid mask plus the team mask.
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

	void OnJump( float fImpulse );
	void OnLand( float fVelocity );

	bool HasC4() const;	// Is this player carrying a C4 bomb?
	bool IsVIP() const;
	bool IsCloseToActiveBomb();
	bool IsCloseToHostage();
	bool IsObjectiveKill( CCSPlayer* pCSVictim );

	int GetClass( void ) const;

	void MakeVIP( bool isVIP );

	virtual void SetAnimation( PLAYER_ANIM playerAnim );
	void DoAnimStateEvent( PlayerAnimEvent_t evt );

	virtual bool StartReplayMode( float fDelay, float fDuration, int iEntity ) OVERRIDE;
	virtual void StopReplayMode() OVERRIDE;
	virtual void PlayUseDenySound() OVERRIDE;

	bool IsOtherSameTeam( int nTeam );
	bool IsOtherEnemy( CCSPlayer *pPlayer );
	bool IsOtherEnemy( int nEntIndex );
	bool IsOtherEnemyAndPlaying( int nEntIndex ); // Doesn't consider observer to be an enemy

public:

	// Simulates a single frame of movement for a player
	void RunPlayerMove( const QAngle& viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, float frametime );
	virtual void HandleAnimEvent( animevent_t *pEvent );

	virtual void UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  );
	virtual void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );

	// from cbasecombatcharacter
	void InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );
	void VPhysicsShadowUpdate( IPhysicsObject *pPhysics );
	
	bool HasWeaponOfType( int nWeaponID ) const;
	bool IsPrimaryOrSecondaryWeapon( CSWeaponType nType );

	virtual bool IsTaunting( void ) const { return m_bIsTaunting; }
	virtual bool IsThirdPersonTaunt( void ) const { return m_bIsThirdPersonTaunt; }
	virtual float GetTauntYaw( void ) const { return m_flTauntYaw; }
	virtual void StopTaunting( void ) { m_bIsTaunting = false; }

	virtual bool IsLookingAtWeapon( void ) const { return m_bIsLookingAtWeapon; }
	virtual bool IsHoldingLookAtWeapon( void ) const { return m_bIsHoldingLookAtWeapon; }
	virtual void StopLookingAtWeapon( void ) { m_bIsLookingAtWeapon = false; m_bIsHoldingLookAtWeapon = false; }
	void ModifyTauntDuration( float flTimingChange );

	CBaseEntity *GetUsableHighPriorityEntity( void );
	bool GetUseConfigurationForHighPriorityUseEntity( CBaseEntity *pEntity, CConfigurationForHighPriorityUseEntity_t &cfg );

	bool HasShield() const;
	bool IsShieldDrawn() const;
	void GiveShield( void );
	void RemoveShield( void );
	bool IsProtectedByShield( void ) const;		// returns true if player has a shield and is currently hidden behind it

	bool HasPrimaryWeapon( void );
	bool HasSecondaryWeapon( void );

	bool IsReloading( void ) const;				// returns true if current weapon is reloading

	void GiveDefaultItems();
	void GiveDefaultWearables();
	void GiveWearableFromSlot( loadout_positions_t position );
	void RemoveAllItems( bool removeSuit );	//overridden to remove the defuser
	void ValidateWearables( void );

	// Reset account, get rid of shield, etc..
	void Reset( bool resetScore );

	void RoundRespawn( void );
	void ObserverRoundRespawn( void );
	void CheckTKPunishment( void );

	virtual void ObserverUse( bool bIsPressed ); // observer pressed use

	// Add money to this player's account.
	void ResetAccount();
	void InitializeAccount( int amount = -1 );
	void AddAccount( int amount, bool bTrackChange = true, bool bItemBought = false, const char *pItemName = NULL );
	bool AreAccountAwardsEnabled( PlayerCashAward::Type reason ) const;
	void AddAccountAward( PlayerCashAward::Type reason );
	void AddAccountAward( PlayerCashAward::Type reason, int amount, const CWeaponCSBase *pWeapon = NULL );
	void AddAccountFromTeam( int amount, bool bTrackChange, TeamCashAward::Type reason );

	int AddDeathmatchKillScore( int nScore, CSWeaponID wepID, int iSlot, bool bIsAssist = false, const char* szVictim = NULL );

	void HintMessage( const char *pMessage, bool bDisplayIfDead, bool bOverrideClientSettings = false ); // Displays a hint message to the player
	CHintMessageQueue *m_pHintMessageQueue;
	unsigned int m_iDisplayHistoryBits;
	bool m_bShowHints;
	float m_flLastAttackedTeammate;
	float m_flNextMouseoverUpdate;
	void UpdateMouseoverHints();

	float m_flDominateEffectDelayTime;
	EHANDLE m_hDominateEffectPlayer;

	// mark this player as not receiving money at the start of the next round.
	void ProcessSuicideAsKillReward();
	void MarkAsNotReceivingMoneyNextRound( bool bAllowMoneyNextRound = false );
	bool DoesPlayerGetRoundStartMoney(); // self-explanitory :)

	virtual bool ShouldPickupItemSilently( CBaseCombatCharacter *pNewOwner );

	void DropC4();	// Get rid of the C4 bomb.
	
	CNetworkHandle( CBaseEntity, m_hCarriedHostage );	// networked entity handle 
	//EHANDLE GetCarriedHostage() const;
	void GiveCarriedHostage( EHANDLE hHostage );
	void RefreshCarriedHostage( bool bForceCreate );
	void RemoveCarriedHostage();
	CNetworkHandle( CBaseEntity, m_hCarriedHostageProp );	// networked entity handle 
	EHANDLE	m_hHostageViewModel;

	bool HasDefuser();		// Is this player carrying a bomb defuser?
	void GiveDefuser( bool bPickedUp = false );		// give the player a defuser
	void RemoveDefuser();	// remove defuser from the player and remove the model attachment

	bool PickedUpDefuser() { return m_bPickedUpDefuser; }
	void SetDefusedWithPickedUpKit(bool bDefusedWithPickedUpKit) { m_bDefusedWithPickedUpKit = bDefusedWithPickedUpKit; }
	bool GetDefusedWithPickedUpKit() { return m_bDefusedWithPickedUpKit; }
	bool AttemptedToDefuseBomb() { return m_bAttemptedDefusal; }

	void SetDefusedBombWithThisTimeRemaining( float flTimeRemaining ) { m_flDefusedBombWithThisTimeRemaining = flTimeRemaining; }
	float GetDefusedBombWithThisTimeRemaining() { return m_flDefusedBombWithThisTimeRemaining; }

	bool IsBlindForAchievement();	// more stringent than IsBlind; more accurately represents when the player can see again

	// [jpaquin] auto refill ammo
	float m_flNextAutoBuyAmmoTime;
	void AutoBuyAmmo( bool bForce = false );
	void GuardianForceFillAmmo( void );

	bool IsBlind( void ) const;		// return true if this player is blind (from a flashbang)
	virtual void Blind( float holdTime, float fadeTime, float startingAlpha = 255 );	// player blinded by a flashbang
	void Unblind( void );	// removes the blind effect from the player
	float m_blindUntilTime;
	float m_blindStartTime;

	void Deafen( float flDistance );		//make the player deaf / apply dsp preset to muffle sound

	void ApplyDeafnessEffect();				// apply the deafness effect for a nearby explosion.

	bool IsAutoFollowAllowed( void ) const;		// return true if this player will allow bots to auto follow
	void InhibitAutoFollow( float duration );	// prevent bots from auto-following for given duration
	void AllowAutoFollow( void );				// allow bots to auto-follow immediately
	float m_allowAutoFollowTime;				// bots can auto-follow after this time

	// Have this guy speak a message into his radio.
	void Radio( const char *szRadioSound, const char *szRadioText = NULL , bool bTriggeredAutomatically = false );
	void ConstructRadioFilter( CRecipientFilter& filter );
	float m_flGotHostageTalkTimer;
	float m_flDefusingTalkTimer;
	float m_flC4PlantTalkTimer;

	virtual bool CanHearAndReadChatFrom( CBasePlayer *pPlayer );

	void EmitPrivateSound( const char *soundName );		///< emit given sound that only we can hear

	bool Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	CWeaponCSBase* GetActiveCSWeapon() const;

	int GetNumTriggerPulls() { return m_triggerPulls; }
	void LogTriggerPulls();

	void PreThink();

	// This is the think function for the player when they first join the server and have to select a team
	void JoiningThink();

	virtual bool ClientCommand( const CCommand &args );

	bool AllowTaunts( void );
	void Taunt( void );
	void LookAtHeldWeapon( void );

	void SendJoinTeamFailedMessage( int reason, bool raiseTeamScreen );

	bool HandleCommand_JoinClass( void );
	bool HandleCommand_JoinTeam( int iTeam, bool bQueue = false, int iCoachTeam = 0 ); // if bQueue is true then the team move will not occur until round restart

	BuyResult_e HandleCommand_Buy( const char *item, int nPos = -1, bool bAddToRebuy = true );
	BuyResult_e HandleCommand_Buy_Internal( const char * item, int nPos = -1, bool bAddToRebuy = true  );

	CNetworkVar( bool, m_bIsBuyMenuOpen );
	bool IsBuyMenuOpen( void ) { return m_bIsBuyMenuOpen; } 
	void SetBuyMenuOpen( bool bOpen );

	AcquireResult::Type CanAcquire( CSWeaponID weaponId, AcquireMethod::Type acquireMethod, CEconItemView *pItem = NULL );
	int					GetCarryLimit( CSWeaponID weaponId );
	int	GetWeaponPrice( CSWeaponID weaponId, const CEconItemView *pWepView = NULL ) const;
	CWeaponCSBase*		CSWeapon_OwnsThisType( CEconItemView *pItem ) const;

	void HandleMenu_Radio1( int slot );
	void HandleMenu_Radio2( int slot );
	void HandleMenu_Radio3( int slot );

	float m_flRadioTime;	
	int m_iRadioMessages;
	int iRadioMenu;

	void ListPlayers();

	bool m_bIgnoreRadio;

	// Returns one of the CS_CLASS_ enums.
	int PlayerClass() const;

	void MoveToNextIntroCamera();

	// Used to be GETINTOGAME state.
	void GetIntoGame();

	CBaseEntity* EntSelectSpawnPoint();
	
	void SetProgressBarTime( int barTime );
	virtual void PlayerDeathThink();
	virtual void PlayerForceTeamThink();
	virtual void ResetForceTeamThink();

	virtual bool StartObserverMode( int mode ) OVERRIDE;
	virtual bool SetObserverTarget( CBaseEntity *target );
	virtual void ValidateCurrentObserverTarget( void );
	virtual void CheckObserverSettings( void );

	void Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );
	virtual bool Weapon_CanUse( CBaseCombatWeapon *pWeapon );

	void ClearFlashbangScreenFade ( void );
	bool ShouldDoLargeFlinch( const CTakeDamageInfo& info, int nHitGroup );

	void ResetStamina( void );
	bool IsArmored( int nHitGroup );
	void Pain( CCSPlayer* attacker, bool HasArmour, int nDmgTypeBits = 0 );
	
	void DeathSound( const CTakeDamageInfo &info );
	
	bool Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );
	virtual void OnSwitchWeapons( CBaseCombatWeapon* pWeapon );
	void ChangeTeam( int iTeamNum );
	void SwitchTeam( int iTeamNum );	// Changes teams without penalty - used for auto team balancing

	virtual void ModifyOrAppendCriteria( AI_CriteriaSet& set );
	void ModifyOrAppendPlayerCriteria( AI_CriteriaSet& set );

	virtual void OnDamagedByExplosion( const CTakeDamageInfo &info );

	virtual bool CanKickFromTeam( int kickTeam );


	// Called whenever this player fires a shot.
	void NoteWeaponFired();
	virtual bool WantsLagCompensationOnEntity( const CBaseEntity *pPlayer, const CUserCmd *pCmd, const CBitVec<MAX_EDICTS> *pEntityTransmitBits ) const;

	// training map - used to set hud element visibility via script
	void SetMiniScoreHidden( bool bHidden ) { m_bHud_MiniScoreHidden = bHidden; }
	bool IsMiniScoreHidden( void ) { return m_bHud_MiniScoreHidden; }
	void SetRadarHidden( bool bHidden ) { m_bHud_RadarHidden = bHidden; }
	bool IsRadarHidden( void ) { return m_bHud_RadarHidden; }
	
	void SetLastKillerIndex( int nLastKillerIndex ) { m_nLastKillerIndex = nLastKillerIndex; }
	int GetLastKillerIndex( void ) { return m_nLastKillerIndex; }
	void SetLastConcurrentKilled( int nLastConcurrentKilled ) { m_nLastConcurrentKilled = nLastConcurrentKilled; }
	int GetLastConcurrentKilled( void ) { return m_nLastConcurrentKilled; }
	void SetDeathCamMusicIndex( int nDeathCamMusicIndex ) { m_nDeathCamMusic = nDeathCamMusicIndex; }
	int GetDeathCamMusicIndex( void ) { return m_nDeathCamMusic; }

	void UpdateEquippedCoinFromInventory();
	void SetRank( MedalCategory_t category, MedalRank_t rank );
	MedalRank_t GetRank( MedalCategory_t category ) { return m_rank.Get( category ); }
	void UpdateRankFromKV( KeyValues *pKV );

	void UpdateEquippedMusicFromInventory();
	void SetMusicID( uint16 unMusicID );
	uint32 GetMusicID( ) { return m_unMusicID.Get(); }

	void UpdateEquippedPlayerSprayFromInventory();

	void UpdatePersonaDataFromInventory();
	CEconPersonaDataPublic const * GetPersonaDataPublic() const;

	virtual void AwardAchievement( int iAchievement, int iCount = 1 );


// ------------------------------------------------------------------------------------------------ //
// Player state management.
// ------------------------------------------------------------------------------------------------ //
public:

	void State_Transition( CSPlayerState newState );	// Cleanup the previous state and enter a new state.
	CSPlayerState State_Get() const;				// Get the current state.

private:
	void State_Enter( CSPlayerState newState );		// Initialize the new state.
	void State_Leave();								// Cleanup the previous state.
	void State_PreThink();							// Update the current state.
	
	// Find the state info for the specified state.
	static CCSPlayerStateInfo* State_LookupInfo( CSPlayerState state );

	// This tells us which state the player is currently in (joining, observer, dying, etc).
	// Each state has a well-defined set of parameters that go with it (ie: observer is movetype_noclip, nonsolid,
	// invisible, etc).
	CNetworkVar( CSPlayerState, m_iPlayerState );

	CCSPlayerStateInfo *m_pCurStateInfo;			// This can be NULL if no state info is defined for m_iPlayerState.

	// tells us whether or not this player gets money at the start of the next round.
	bool m_receivesMoneyNextRound;

	// Specific state handler functions.
	void State_Enter_WELCOME();
	void State_PreThink_WELCOME();

	void State_Enter_PICKINGTEAM();
	void State_Enter_PICKINGCLASS();

	void State_Enter_ACTIVE();
	void State_PreThink_ACTIVE();

	void State_Enter_OBSERVER_MODE();
	void State_Leave_OBSERVER_MODE();
	void State_PreThink_OBSERVER_MODE();

	void State_Enter_GUNGAME_RESPAWN();
	void State_PreThink_GUNGAME_RESPAWN();
	void TryGungameRespawn();

	void State_Enter_DEATH_WAIT_FOR_KEY();
	void State_PreThink_DEATH_WAIT_FOR_KEY();

	void State_Enter_DEATH_ANIM();
	void State_PreThink_DEATH_ANIM();

	int FlashlightIsOn( void );
	virtual bool FlashlightTurnOn( bool playSound = false );
	virtual void FlashlightTurnOff( bool playSound = false );

	void UpdateAddonBits();
	void ProcessSpottedEntityUpdate();
	void AppendSpottedEntityUpdateMessage( int entindex, bool bSpotted,
		CCSUsrMsg_ProcessSpottedEntityUpdate::SpottedEntityUpdate *pMsg );
	//void UpdateTeamMoney();
	int GetAccountForScoreboard();

public:
	bool StartHltvReplayEvent( const ClientReplayEventParams_t &params );
	int		GetHealth() const		{ return m_iHealth; }
	virtual void	SetHealth( int amt );

	void				SetDeathPose( const int &iDeathPose ) { m_iDeathPose = iDeathPose; }
	void				SetDeathPoseFrame( const int &iDeathPoseFrame ) { m_iDeathFrame = iDeathPoseFrame; }
	void				SetDeathPoseYaw( const float &flDeathPoseYaw ) { m_flDeathYaw = flDeathPoseYaw; }
	
	void				SelectDeathPose( const CTakeDamageInfo &info );

	bool				MadeFinalGunGameProgressiveKill( void ) { return m_bMadeFinalGunGameProgressiveKill; }
	bool				JustLeftSpawnImmunity(){return m_fJustLeftImmunityTime > 0.0f;}

	virtual void		IncrementFragCount( int nCount, int nHeadshots = 0 );
	virtual void		IncrementAssistsCount( int nCount );
	virtual void		IncrementDeathCount( int nCount );
	void				IncrementTeamKillsCount( int nCount );
	void				IncrementHostageKillsCount( int nCount );
	void				IncrementTeamDamagePoints( int numDamagePoints );

	void	SetLastKillTime( float time );
	float	GetLastKillTime();
	void	IncrementKillStreak( int nCount );
	void	ResetKillStreak();
	int		GetKillStreak();

	void	ResetNumRoundKills() { m_iNumRoundKills = 0; m_iNumRoundKillsHeadshots = 0; }
	
	int GetNumConcurrentDominations( void );
	void GiveWeaponFromID ( int nWeaponID );

private:
	int		m_iKillStreak;
	float	m_flLastKillTime;

	int	m_iDeathPose;
	int	m_iDeathFrame;
	float m_flDeathYaw;
	bool m_bAbortFreezeCam;
	bool m_bJustBecameSpectator;

	bool m_bRespawning;
	int m_iNumGunGameTRBombTotalPoints;
	bool m_bShouldProgressGunGameTRBombModeWeapon;
	bool m_switchTeamsOnNextRoundReset;
	float m_fJustLeftImmunityTime;
	float m_lowHealthGoalTime;

	CNetworkArray( MedalRank_t, m_rank, MEDAL_CATEGORY_COUNT );
	bool m_bNeedToUpdateCoinFromInventory;

	CNetworkVar( uint16, m_unMusicID );
	bool m_bNeedToUpdateMusicFromInventory;

	uint16 m_unEquippedPlayerSprayIDs[ /*LOADOUT_POSITION_SPRAY3*/ LOADOUT_POSITION_SPRAY0 + 1 - LOADOUT_POSITION_SPRAY0 ];
	bool m_bNeedToUpdatePlayerSprayFromInventory;

	CEconPersonaDataPublic *m_pPersonaDataPublic;
	bool m_bNeedToUpdatePersonaDataPublicFromInventory;

protected:
	void AttemptToExitFreezeCam( void );

	void GiveCurrentProgressiveGunGameWeapon( void );
	void GiveNextProgressiveGunGameWeapon( void );
	void SubtractProgressiveWeaponIndex( void );

	void SendGunGameWeaponUpgradeAlert( void );

public:

	CNetworkVar( bool, m_bIsScoped );
	CNetworkVar( bool, m_bIsWalking );
	// Predicted variables.
	CNetworkVar( bool, m_bResumeZoom );
	CNetworkVar( bool, m_bIsDefusing );			// tracks whether this player is currently defusing a bomb
	bool IsDefusing( void ) { return m_bIsDefusing; }
	CNetworkVar( bool, m_bIsGrabbingHostage );			// tracks whether this player is currently grabbing a hostage
	CNetworkVar( float, m_fImmuneToGunGameDamageTime );	// When gun game spawn damage immunity will expire
	CNetworkVar( bool, m_bGunGameImmunity );	// tracks whether this player is currently immune in gun game
	CNetworkVar( bool, m_bMadeFinalGunGameProgressiveKill );
	CNetworkVar( int,  m_iGunGameProgressiveWeaponIndex );	// index of current gun game weapon
	CNetworkVar( int, m_iNumGunGameTRKillPoints );	// number of kill points accumulated so far in TR Gun Game mode (resets to 0 when weapon is upgraded)
	CNetworkVar( int, m_iNumGunGameKillsWithCurrentWeapon );
	CNetworkVar( int, m_iNumRoundKills );	// number of kills a player has in a single round
	CNetworkVar( int, m_iNumRoundKillsHeadshots ); // number of kills a player got this round with headshots
	int m_iNumRoundTKs;	// number of teammate kills a player has in a single round
	CNetworkVar( float, m_fMolotovUseTime );	// Molotov can be used if current time is after this time
	CNetworkVar( float, m_fMolotovDamageTime );	// Last time when this player was burnt by Molotov damage

	CNetworkVar( bool, m_bHasMovedSinceSpawn );		// Whether player has moved from spawn position

	CNetworkVar( bool, m_bCanMoveDuringFreezePeriod );		

	CNetworkVar( bool, m_isCurrentGunGameLeader );		
	CNetworkVar( bool, m_isCurrentGunGameTeamLeader );	
	
	CNetworkVar( float, m_flGuardianTooFarDistFrac );
	float m_flNextGuardianTooFarHurtTime;

	CNetworkVar( float, m_flDetectedByEnemySensorTime );	

	void GiveHealthAndArmorForGuardianMode( bool bAdditive );
	int GetPlayerGunGameWeaponIndex( void ) { return m_iGunGameProgressiveWeaponIndex; }
	int GetNumGunGameTRKillPoints( void ) { return m_iNumGunGameTRKillPoints; }
	int GetNumRoundKills( void ) { return m_iNumRoundKills; }
	int GetNumRoundKillsHeadshots( void ) { return m_iNumRoundKillsHeadshots; }
	int GetNumGunGameKillsWithCurrentWeapon( void ) { return m_iNumGunGameKillsWithCurrentWeapon; }

	//--------------------------------------------------------------------------------------------------------
	void OnHealthshotUsed( void );
	bool UpdateTeamLeaderPlaySound( int nTeam );
	void UpdateLeader( void );

	void IncrementGunGameProgressiveWeapon( int nNumLevelsToIncrease );

	// [tj] overriding the base suicides to trash CS specific stuff
	virtual void CommitSuicide( bool bExplode = false, bool bForce = false );
	virtual void CommitSuicide( const Vector &vecForce, bool bExplode = false, bool bForce = false );

	void WieldingKnifeAndKilledByGun( bool bState ) { m_bWieldingKnifeAndKilledByGun = bState; }
	bool WasWieldingKnifeAndKilledByGun() { return m_bWieldingKnifeAndKilledByGun; }
	
	void RecordRebuyStructLastRound();

	void HandleEndOfRound();
	bool WasKilledThisRound(){return m_wasKilledThisRound;}
	void SetWasKilledThisRound(bool wasKilled);
	int GetMaxNumRoundsSurvived() {return m_maxNumRoundsSurvived;}
	int GetCurNumRoundsSurvived() {return m_numRoundsSurvived;}
	
	// [dwenger] adding tracking for weapon used fun fact
	void PlayerUsedFirearm( CBaseCombatWeapon* pBaseWeapon );
	void PlayerEmptiedAmmoForFirearm( CBaseCombatWeapon* pBaseWeapon );
	void PlayerOutOfAmmoForFirearm( CBaseCombatWeapon* pBaseWeapon );
	void PlayerHeldFirearm( CBaseCombatWeapon* pBaseWeapon );
	void AddBurnDamageDelt( int entityIndex);
	int GetNumPlayersDamagedWithFire();

	int GetNumFirearmsHeld() { return m_WeaponTypesHeld.Count(); }
	int GetNumFirearmsUsed() { return m_WeaponTypesUsed.Count(); }
	int GetNumFirearmsRanOutOfAmmo() { return m_WeaponTypesRunningOutOfAmmo.Count(); }
	bool DidPlayerEmptyAmmoForWeapon( CBaseCombatWeapon* pBaseWeapon );
	void SetLastWeaponBeforeAutoSwitchToC4( CBaseCombatWeapon * weapon ){m_lastWeaponBeforeC4AutoSwitch = weapon;}
	CBaseCombatWeapon* GetLastWeaponBeforeAutoSwitchToC4( void ){return m_lastWeaponBeforeC4AutoSwitch;}
	void RestoreWeaponOnC4Abort();

	void PlayerUsedGrenade( int nWeaponID );
	void PlayerUsedKnife( void );

	void ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName );

	bool HasHeavyArmor() const	{return m_bHasHeavyArmor;}
	CNetworkVar( bool, m_bHasHelmet );				// Does the player have helmet armor
	CNetworkVar( bool, m_bHasHeavyArmor );				// Does the player have heavy armor?
	bool m_bEscaped;			// Has this terrorist escaped yet?

	// Other variables.
	bool m_bIsVIP;				// Are we the VIP?
	int m_iNumSpawns;			// Number of times player has spawned this round
	int m_iOldTeam;				// Keep what team they were last on so we can allow joining spec and switching back to their real team
	bool m_bTeamChanged;		// Just allow one team change per round
	int m_iShouldHaveCash;

	bool m_bHasSeenJoinGame;	// Since creation, have we seen a joingame command for this player?

	int m_iLastTeam;			// Last team on which this player was a member
	
	bool m_bJustKilledTeammate;
	bool m_bPunishedForTK;
	bool m_bInvalidSteamLogonDelayed;
	int m_iTeamKills;
	float m_flLastAction;
	int m_iNextTimeCheck;		// Next time the player can execute a "timeleft" command

	float m_flNameChangeHistory[NAME_CHANGE_HISTORY_SIZE]; // index 0 = most recent change

	bool CanChangeName( void );	// Checks if the player can change his name
	void ChangeName( const char *pszNewName );

	void SetClanTag( const char *pTag );
	const char *GetClanTag( void ) const;
	void SetClanName( const char *pName );
	const char *GetClanName( void ) const;

	void InitTeammatePreferredColor();
	void SetTeammatePreferredColor( int nColor );
	int GetTeammatePreferredColor( void ) const;

	//RecvPropEHandle( RECVINFO(m_hCarriedHostage) ),

	CNetworkVar( bool, m_bHasDefuser );			// Does this player have a defuser kit?
	float m_fLastGivenDefuserTime;				// the last time this player received the defuser
	float m_fLastGivenBombTime;					// the last time this player received the bomb
	CNetworkVar( bool, m_bHasNightVision );		// Does this player have night vision?
	CNetworkVar( bool, m_bNightVisionOn );		// Is the NightVision turned on ?

	float m_fNextRadarUpdateTime;
	float m_flLastMoneyUpdateTime;

	// Backup copy of the menu text so the player can change this and the menu knows when to update.
	char	m_MenuStringBuffer[MENU_STRING_BUFFER_SIZE];

	// When the player joins, it cycles their view between trigger_camera entities.
	// This is the current camera, and the time that we'll switch to the next one.
	EHANDLE m_pIntroCamera;
	float m_fIntroCamTime;

	// Set to true each frame while in a bomb zone.
	// Reset after prediction (in PostThink).
	CNetworkVar( bool, m_bInBombZone );
	CNetworkVar( bool, m_bInBuyZone );
	// See if we need to prevent player from being able to diffuse bomb.
	CNetworkVar( bool, m_bInNoDefuseArea );
	CNetworkVar( bool, m_bKilledByTaser );
	CNetworkVar( int, m_iMoveState );		// Is the player trying to run?  Used for state transitioning after a player lands from a jump etc.
	
	CNetworkString( m_szArmsModel, MAX_MODEL_STRING_SIZE );		// Which arms we're using for the view model.

	// Match Stats data
	CNetworkArray( int, m_iMatchStats_Kills,			MAX_MATCH_STATS_ROUNDS );				//kills, per round
	CNetworkArray( int, m_iMatchStats_Damage,			MAX_MATCH_STATS_ROUNDS );				//damage, per round
	CNetworkArray( int, m_iMatchStats_EquipmentValue,	MAX_MATCH_STATS_ROUNDS );				//Equipment value, per round
	CNetworkArray( int, m_iMatchStats_MoneySaved,		MAX_MATCH_STATS_ROUNDS );				//Saved money, per round
	CNetworkArray( int, m_iMatchStats_KillReward,		MAX_MATCH_STATS_ROUNDS );				//Money earned from kills, per round
	CNetworkArray( int, m_iMatchStats_LiveTime,			MAX_MATCH_STATS_ROUNDS );				//Time spent alive, per round
	CNetworkArray( int, m_iMatchStats_Deaths,			MAX_MATCH_STATS_ROUNDS );				//Deaths
	CNetworkArray( int, m_iMatchStats_Assists,			MAX_MATCH_STATS_ROUNDS );				//Assists
	CNetworkArray( int, m_iMatchStats_HeadShotKills,	MAX_MATCH_STATS_ROUNDS );				// Head shot kills
	CNetworkArray( int, m_iMatchStats_Objective,		MAX_MATCH_STATS_ROUNDS );				// successful objectives ( bomb plants, defuses, hostage rescues )
	CNetworkArray( int, m_iMatchStats_CashEarned,		MAX_MATCH_STATS_ROUNDS );				// Cash awards
	CNetworkArray( int, m_iMatchStats_UtilityDamage,	MAX_MATCH_STATS_ROUNDS );				// Grenade etc damage
	CNetworkArray( int, m_iMatchStats_EnemiesFlashed,	MAX_MATCH_STATS_ROUNDS );				// Enemies flashed

	bool m_bUseNewAnimstate;
	virtual void SetModel( const char *szModelName );

	virtual Vector Weapon_ShootPosition();

	int m_iBombSiteIndex;
	// keep track of when we enter and exit a bombzone
	bool	m_bInBombZoneTrigger;
	bool	m_bWasInBombZoneTrigger;
	bool	m_bWasInHostageRescueZone;
	// keep track of when we enter and exit a buyzone
	bool m_bWasInBuyZone;

	bool IsInBuyZone();
	bool IsInBuyPeriod();
	bool CanBuyDuringImmunity();
	bool CanPlayerBuy( bool display );

	CNetworkVar( bool, m_bInHostageRescueZone );
	void RescueZoneTouch( inputdata_t &inputdata );

	CNetworkVar( float, m_flStamina );
	CNetworkVar( int, m_iDirection );	// The current lateral kicking direction; 1 = right,  0 = left
	CNetworkVar( int, m_iShotsFired );	// number of shots fired recently (seems inconsistent, based on specific weapons incrementing this value)
	CNetworkVar( int, m_nNumFastDucks );  // UNUSED.  Kept for backwards demo compatibility.  $$$REI TODO: Investigate safely removing variables
	CNetworkVar( bool, m_bDuckOverride ); // force the player to duck regardless of if they're holding crouch

	// Make sure to register changes for armor.
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_ArmorValue );

	float m_flFlinchStack; // we add to this stack everytime we take damage that would "tag" us - decays constantly
	CNetworkVar( float, m_flVelocityModifier );
	void SetFlinchVelocityModifier( float fVelocityModifier )
	{
		// this function only allows more flinch (smaller values) to be applied, not less
		m_flVelocityModifier = Min(m_flVelocityModifier.Get(), fVelocityModifier);
	}
	CNetworkVar( float, m_flGroundAccelLinearFracLastTime );

	int	m_iHostagesKilled;
	int m_bulletsFiredSinceLastSpawn;

	void SetShieldDrawnState( bool bState );
	void DropShield( void );
	
	char m_szNewName [MAX_PLAYER_NAME_LENGTH]; // not empty if player requested a namechange
	char m_szClanTag[MAX_CLAN_TAG_LENGTH];
	char m_szClanName[MAX_TEAM_NAME_LENGTH];

	Vector m_vecTotalBulletForce;	//Accumulator for bullet force in a single frame
	
	// preferred teammate color that user has set for themselves
	int m_iTeammatePreferredColor;

	CNetworkVar( float, m_flFlashDuration );
	CNetworkVar( float, m_flFlashMaxAlpha );
	
	CNetworkVar( float, m_flProgressBarStartTime );
	CNetworkVar( int, m_iProgressBarDuration );
	CNetworkVar( int, m_iThrowGrenadeCounter );	// used to trigger grenade throw animations.
	CNetworkVar( bool, m_bWaitForNoAttack );	// flag to indicate player cannot attack until the attack button is released
	CNetworkVar( bool, m_bIsRespawningForDMBonus ); // flag to indicate player wants to spawn with the deathmatch bonus weapons
	
	CNetworkVar( float, m_flLowerBodyYawTarget );
	CNetworkVar( bool, m_bStrafing );

	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle 

	// Bots and hostages auto-duck during jumps
	bool m_duckUntilOnGround;

	Vector m_lastStandingPos; // used by the gamemovement code for finding ladders

	void SurpressLadderChecks( const Vector& pos, const Vector& normal );
	bool CanGrabLadder( const Vector& pos, const Vector& normal );

	void ClearGunGameImmunity( void );
	void ClearGunGameProgressiveWeaponIndex( void ) { m_iGunGameProgressiveWeaponIndex = 0; }
	void ResetTRBombModeWeaponProgressFlag( void ) { m_bShouldProgressGunGameTRBombModeWeapon = false; }
	void ResetTRBombModeKillPoints( void ) { m_iNumGunGameTRKillPoints = 0; }
	void SwitchTeamsAtRoundReset( void ) { m_switchTeamsOnNextRoundReset = true; }
	bool WillSwitchTeamsAtRoundReset( void ) { return m_switchTeamsOnNextRoundReset; }
	void ResetTRBombModeData( void );

	void SetPickedUpWeaponThisRound( bool pickedUp ){m_bPickedUpWeapon = pickedUp;}
	bool GetPickedUpWeaponThisRound( void ){return m_bPickedUpWeapon;}

	bool CanUseGrenade( CSWeaponID );

	bool CSWeaponDrop( CBaseCombatWeapon *pWeapon, bool bDropShield = true, bool bThrow = false );
	bool CSWeaponDrop( CBaseCombatWeapon *pWeapon, Vector targetPos, bool bDropShield = true );

	bool HandleDropWeapon( CBaseCombatWeapon *pWeapon = NULL, bool bSwapping = false );

	void SetViewModelArms( const char *armsModel );

	void ReportCustomClothingModels( void );

	void DestroyWeapon( CBaseCombatWeapon *pWeapon );

	void DestroyWeapons( bool bDropC4 = true );

	bool IsPlayerSpawning( void ) { return m_bIsSpawning; }
	void SetPlayerSpawning( bool bIsSpawning ) { m_bIsSpawning = bIsSpawning; }

private:
	CountdownTimer m_ladderSurpressionTimer;
	Vector m_lastLadderNormal;
	Vector m_lastLadderPos;

protected:

	void CreateRagdollEntity();

	bool IsHittingShield( const Vector &vecDirection, trace_t *ptr );

	void PhysObjectSleep();
	void PhysObjectWake();

	bool RunMimicCommand( CUserCmd& cmd );

	bool SelectSpawnSpot( const char *pEntClassName, CBaseEntity* &pSpot );

	void SetModelFromClass( void );
	CNetworkVar( int, m_iClass ); // One of the CS_CLASS_ enums.

	void TransferInventory( CCSPlayer* pTargetPlayer );
	bool DropWeaponSlot( int nSlot, bool fromDeath = false );

	void DropWeapons( bool fromDeath, bool killedByEnemy );

	virtual int SpawnArmorValue( void ) const { return ArmorValue(); }

	bool BAttemptToBuyCheckSufficientBalance( int nCostOfPurchaseToCheck, bool bClientPrint = true );
	BuyResult_e AttemptToBuyAmmo( int iAmmoType );
	BuyResult_e AttemptToBuyAmmoSingle( int iAmmoType );
	BuyResult_e AttemptToBuyVest( void );
	BuyResult_e AttemptToBuyAssaultSuit( void );
	BuyResult_e AttemptToBuyHeavyAssaultSuit( void );
	BuyResult_e AttemptToBuyDefuser( void );
	BuyResult_e AttemptToBuyNightVision( void );
	
	BuyResult_e BuyAmmo( int nSlot, bool bBlinkMoney );
	BuyResult_e BuyGunAmmo( CBaseCombatWeapon *pWeapon, bool bBlinkMoney );

	void InternalAutoBuyAmmo( int slot );

	void PushawayThink();

private:

	IPlayerAnimState *m_PlayerAnimState;
	CCSGOPlayerAnimState *m_PlayerAnimStateCSGO;

	// Aiming heuristics code
	float						m_flIdleTime;		//Amount of time we've been motionless
	float						m_flMoveTime;		//Amount of time we've been in motion
	float						m_flLastDamageTime;	//Last time we took damage
	float						m_flTargetFindTime;

	int							m_lastDamageHealth;		// Last damage given to our health
	int							m_lastDamageArmor;		// Last damage given to our armor

	
	bool						m_bPickedUpWeapon;
	bool						m_bPickedUpDefuser;			// Did player pick up the defuser kit as opposed to buying it?
	bool						m_bDefusedWithPickedUpKit;	// Did player defuse the bomb with a picked-up defuse kit?
	bool						m_bAttemptedDefusal;
	int							m_nPreferredGrenadeDrop;

	float						m_flDefusedBombWithThisTimeRemaining;

	// Last usercmd we shot a bullet on.
	int m_iLastWeaponFireUsercmd;

	// Copyed from EyeAngles() so we can send it to the client.
	CNetworkVectorXYZ( m_angEyeAngles );

	bool m_bVCollisionInitted;

	Vector m_storedSpawnPosition;
	QAngle m_storedSpawnAngle;
	bool m_bIsSpawning;

public:
	CNetworkVar( float, m_flThirdpersonRecoil );

// AutoBuy functions.
public:
	void			AutoBuy( const char *autobuyString ); // this should take into account what the player can afford and should buy the best equipment for them.

	bool			IsInAutoBuy( void ) { return m_bIsInAutoBuy; }
	bool			IsInReBuy( void ) { return m_bIsInRebuy; }
	int				GetAccountBalance( );

private:
	bool			ShouldExecuteAutoBuyCommand(const AutoBuyInfoStruct *commandInfo, bool boughtPrimary, bool boughtSecondary);
	void			PostAutoBuyCommandProcessing(const AutoBuyInfoStruct *commandInfo, bool &boughtPrimary, bool &boughtSecondary);
	void			ParseAutoBuyString(const char *string, bool &boughtPrimary, bool &boughtSecondary);
	AutoBuyInfoStruct *GetAutoBuyCommandInfo(const char *command);
	void			PrioritizeAutoBuyString(char *autobuyString, const char *priorityString); // reorders the tokens in autobuyString based on the order of tokens in the priorityString.
	BuyResult_e	CombineBuyResults( BuyResult_e prevResult, BuyResult_e newResult );

	bool			m_bIsInAutoBuy;
	bool			m_bAutoReload;

//ReBuy functions

public:
	void			Rebuy( const char *rebuyString );
	bool			AttemptToBuyDMBonusWeapon( void );
private:
	void			AddToRebuy( CSWeaponID weaponId, int nPos );
	void			AddToGrenadeRebuy( CSWeaponID weaponId );

	BuyResult_e	RebuyPrimaryWeapon();
	BuyResult_e	RebuySecondaryWeapon();
	BuyResult_e	RebuyTaser();
	BuyResult_e	RebuyGrenade( CSWeaponID weaponId );
	BuyResult_e	RebuyDefuser();
	BuyResult_e	RebuyNightVision();
	BuyResult_e	RebuyArmor();

	bool			m_bIsInRebuy;
	RebuyStruct		m_rebuyStruct;
	RebuyStruct		m_rebuyStructLastRound;
	bool			m_bUsingDefaultPistol;

	bool			m_bIsBeingGivenItem;

//BuyRandom functions
public:
	void			BuyRandom();


#ifdef CS_SHIELD_ENABLED
	CNetworkVar( bool, m_bHasShield );
	CNetworkVar( bool, m_bShieldDrawn );
#endif
	
	CNetworkVar( bool, m_bHud_MiniScoreHidden );
	CNetworkVar( bool, m_bHud_RadarHidden );

	CNetworkVar( int, m_nLastKillerIndex );
	CNetworkVar( int, m_nLastConcurrentKilled );
	CNetworkVar( int, m_nDeathCamMusic ); // this players deathcam music index

	// This is a combination of the ADDON_ flags in cs_shareddefs.h.
	CNetworkVar( int, m_iAddonBits );

	// Clients don't know about holstered weapons, so we need to tell them the weapon type here
	CNetworkVar( int, m_iPrimaryAddon );
	CNetworkVar( int, m_iSecondaryAddon );

	CNetworkVar( int, m_iAccount );	// How much cash this player has.
	int m_iAccountMoneyEarnedForNextRound;	// How much of this player's cash cannot be used during this round (kill rewards, suicide rewards, etc.)
	CNetworkVar( int, m_iStartAccount );	// How much cash this player started the round with

	CNetworkVar( int, m_totalHitsOnServer ); // used to compare against client's hit counts to see how 'wrong' the client was.

//Damage record functions
public:

	static void	StartNewBulletGroup();	// global function
	static uint32 GetBulletGroup();		// global function
	static void ResetBulletGroup();		// global function, called at the beginning of every match

	void RecordDamage( CCSPlayer* damageDealer, CCSPlayer* damageTaker, int iDamageDealt, int iActualHealthRemoved );

	void ResetDamageCounters();	//Reset all lists

	void RemoveSelfFromOthersDamageCounters(); // Additional cleanup to damage counters when not in a round respawn mode.

	void OutputDamageTaken( void );
	void OutputDamageGiven( void );

	void SendLastKillerDamageToClient( CCSPlayer *pLastKiller );

	void StockPlayerAmmo( CBaseCombatWeapon *pNewWeapon = NULL );

	CUtlLinkedList< CDamageRecord *, int >& GetDamageList() {return m_DamageList;}
	int GetNumAttackersFromDamageList( void );
	int GetMostNumHitsDamageRecordFrom( CCSPlayer *pAttacker );

	int m_nTeamDamageGivenForMatch;
	bool m_bTDGaveProtectionWarning;
	bool m_bTDGaveProtectionWarningThisRound;


private:
	//A unified list of recorded damage that includes giver and taker in each entry
	CUtlLinkedList< CDamageRecord *, int >	m_DamageList;

protected:
	float m_flLastTHWarningTime;
	float m_applyDeafnessTime;
	int m_currentDeafnessFilter;

	bool m_isVIP;

// Command rate limiting.
private:

	bool ShouldRunRateLimitedCommand( const CCommand &args );

	// This lets us rate limit the commands the players can execute so they don't overflow things like reliable buffers.
	CUtlDict<float,int>	m_RateLimitLastCommandTimes;

	CNetworkVar(int, m_cycleLatch);	// Every so often, we are going to transmit our cycle to the client to correct divergence caused by PVS changes
	CountdownTimer m_cycleLatchTimer;


public:
	void ResetRoundBasedAchievementVariables();
	void OnRoundEnd(int winningTeam, int reason);
	void OnPreResetRound();

	void DecrementProgressiveWeaponFromSuicide( void );
	int GetNumEnemyDamagers();
	int GetNumEnemiesDamaged();
	int GetTotalActualHealthRemovedFromEnemies();
	CBaseEntity* GetNearestSurfaceBelow(float maxTrace);

	// Returns the % of the enemies this player killed in the round
	int GetPercentageOfEnemyTeamKilled();

	//List of times of recent kills to check for sprees
	CUtlVector<float>			m_killTimes; 

	//List of all players killed this round
	CUtlVector<CHandle<CCSPlayer> >		m_enemyPlayersKilledThisRound;

	//List of weapons we have used to kill players with this round
	CUtlVector<int>				m_killWeapons; 

	//List of weapons we have used to kill players with this match
	CUtlVector<int>				m_uniqueKillWeaponsMatch;
	bool						m_bLastKillUsedUniqueWeaponMatch;

	int m_NumEnemiesKilledThisSpawn;
	int m_maxNumEnemiesKillStreak;
	int m_NumEnemiesKilledThisRound;
	int m_NumEnemiesAtRoundStart;
	int m_KillingSpreeStartTime;

	int m_NumChickensKilledThisSpawn;

	bool m_bLastKillUsedUniqueWeapon;

 	int m_iRoundsWon;

	float m_firstKillBlindStartTime; //This is the start time of the blind effect during which we got our most recent kill.
	int m_killsWhileBlind;
	int m_bombCarrierkills;
 
	CNetworkVar( bool, m_bIsRescuing );	// tracks whether a player is currently rescuing a hostage :: Networked to provide access at OGS RoundData recording time
	//bool m_bIsRescuing;			// tracks whether this player is currently rescuing a hostage
	bool m_bInjuredAHostage;	// tracks whether this player injured a hostage
	int  m_iNumFollowers;		// Number of hostages following this player
	bool m_bSurvivedHeadshotDueToHelmet;
	bool m_knifeKillBombPlacer;
	bool m_attemptedBombPlace;
	int m_knifeKillsWhenOutOfAmmo;
	bool m_triggerPulled;
	int m_triggerPulls;

	bool m_bHasUsedDMBonusRespawn;

	// mainly fun-fact/achievement related values below
	int GetKnifeKillsWhenOutOfAmmo() { return m_knifeKillsWhenOutOfAmmo; }
	void IncrKnifeKillsWhenOutOfAmmo() { m_knifeKillsWhenOutOfAmmo++; }
	bool HasAttemptedBombPlace(){ return m_attemptedBombPlace; }
	void SetAttemptedBombPlace(){ m_attemptedBombPlace = true; }
	int GetNumBombCarrierKills( void ) { return m_bombCarrierkills; }
	void IncrementNumFollowers() { m_iNumFollowers++; }
	void DecrementNumFollowers() { m_iNumFollowers--; if (m_iNumFollowers < 0) m_iNumFollowers = 0; }
	int GetNumFollowers() { return m_iNumFollowers; }
	void SetIsRescuing(bool in_bRescuing) { m_bIsRescuing = in_bRescuing; }
	bool IsRescuing() { return m_bIsRescuing; }
	void SetInjuredAHostage(bool in_bInjured) { m_bInjuredAHostage = in_bInjured; }
	bool InjuredAHostage() { return m_bInjuredAHostage; }
	bool PlacedBombThisRound() { return (GetBombPlacedTime() >= 0.0f); }
	float GetBombPickuptime() { return m_bombPickupTime; }
	float GetBombPlacedTime() {return m_bombPlacedTime; }
	float GetBombDroppedTime() {return m_bombDroppedTime; }
	void SetBombPickupTime(float time) { m_bombPickupTime = time; }
	void SetBombPlacedTime( float time) { m_bombPlacedTime = time; }
	void SetBombDroppedTime( float time) { m_bombDroppedTime = time; }
	void SetKnifeLevelKilledBombPlacer( void ) { m_knifeKillBombPlacer = true; }
	bool GetKnifeLevelKilledBombPlacer() {return m_knifeKillBombPlacer; }
	CCSPlayer* GetLastFlashbangAttacker() { return m_lastFlashBangAttacker; }
	void SetLastFlashbangAttacker(CCSPlayer* attacker) { m_lastFlashBangAttacker = attacker; }
	float GetKilledTime( void ) { return m_killedTime; }
	void SetKilledTime( float time ); 
	static const CCSWeaponInfo* GetWeaponInfoFromDamageInfo( const CTakeDamageInfo &info );
	static void ProcessPlayerDeathAchievements( CCSPlayer *pAttacker, CCSPlayer *pVictim, const CTakeDamageInfo &info );
	float GetLongestSurvivalTime( void ) { return m_longestLife; }

	void						OnCanceledDefuse();
	void						OnStartedDefuse();
	GooseChaseAchievementStep	m_gooseChaseStep;
	DefuseDefenseAchivementStep	m_defuseDefenseStep;
	CHandle<CCSPlayer>			m_pGooseChaseDistractingPlayer;

	int							m_lastRoundResult; //save the reason for the last round ending.

	bool						m_bMadeFootstepNoise;

	float						m_bombPickupTime;
	float						m_bombPlacedTime;
	float						m_bombDroppedTime;
	float						m_killedTime;
	float						m_spawnedTime;
	float						m_longestLife;
	

	bool						m_bMadePurchseThisRound;

	int							m_roundsWonWithoutPurchase;

	bool						m_bKilledDefuser;
	bool						m_bKilledRescuer;
	int							m_maxGrenadeKills;

	int							m_grenadeDamageTakenThisRound;
	int							m_firstShotKills;
	bool						m_hasReloaded;
	
	
	void						SetHasReloaded(){m_hasReloaded = true;}
	bool						HasReloaded(){return m_hasReloaded;}
	void						IncrementFirstShotKills(int amount){m_firstShotKills += amount;}
	void						ResetFirstShotKills(){m_firstShotKills = 0;}
	int							GetFirstShotKills(){return m_firstShotKills;}

	bool						GetKilledDefuser() { return m_bKilledDefuser; } 
	bool						GetKilledRescuer() { return m_bKilledRescuer; } 
	int							GetMaxGrenadeKills() { return m_maxGrenadeKills; }

	void						CheckMaxGrenadeKills(int grenadeKills);

	CHandle<CCSPlayer>			m_lastFlashBangAttacker;

	void	SetPlayerDominated( CCSPlayer *pPlayer, bool bDominated );    
	void	SetPlayerDominatingMe( CCSPlayer *pPlayer, bool bDominated );
	bool	IsPlayerDominated( int iPlayerIndex );
	bool	IsPlayerDominatingMe( int iPlayerIndex );

	bool	m_wasNotKilledNaturally; //Set if the player is dead from a kill command or late login

	bool	WasNotKilledNaturally() { return m_wasNotKilledNaturally; }

	void	SetNumMVPs( int iNumMVP );
	void	IncrementNumMVPs( CSMvpReason_t mvpReason );
	int		GetNumMVPs();

	void	SetEnemyKillTrackInfo( int iEnemyKills, int iEnemyKillHeadshots, int iEnemy3Ks, int iEnemy4Ks, int iEnemy5Ks, int iEnemyKillsAgg ) { m_iEnemyKills = iEnemyKills; m_iEnemyKillHeadshots = iEnemyKillHeadshots; m_iEnemy3Ks = iEnemy3Ks; m_iEnemy4Ks = iEnemy4Ks; m_iEnemy5Ks = iEnemy5Ks; m_iEnemyKillsAgg = iEnemyKillsAgg; }
	void	GetEnemyKillTrackInfo( int &iEnemyKills, int &iEnemyKillHeadshots, int &iEnemy3Ks, int &iEnemy4Ks, int &iEnemy5Ks, int &iEnemyKillsAgg ) { iEnemyKills = m_iEnemyKills; iEnemyKillHeadshots = m_iEnemyKillHeadshots; iEnemy3Ks = m_iEnemy3Ks; iEnemy4Ks = m_iEnemy4Ks; iEnemy5Ks = m_iEnemy5Ks; iEnemyKillsAgg = m_iEnemyKillsAgg; }

	void	SetEnemyFirstKills( int numFirstKills, int numClutchKills ) { m_numFirstKills = numFirstKills; m_numClutchKills = numClutchKills; }
	void	GetEnemyFirstKills( int &numFirstKills, int &numClutchKills ) { numFirstKills = m_numFirstKills; numClutchKills = m_numClutchKills; }
	void	SetEnemyWeaponKills( int numPistolKills, int numSniperKills ) { m_numPistolKills = numPistolKills; m_numSniperKills = numSniperKills; }
	void	GetEnemyWeaponKills( int &numPistolKills, int &numSniperKills ) { numPistolKills = m_numPistolKills; numSniperKills = m_numSniperKills; }

	void    ClearRoundContributionScore( void ) { m_iRoundContributionScore = 0; }
	void    AddRoundContributionScore( int iPoints );
	int		GetRoundContributionScore( void ) { return m_iRoundContributionScore; }

	void	ClearRoundProximityScore( void ){ m_iRoundProximityScore  = 0; }
	void	AddRoundProximityScore( int iPoints );
	int		GetRoundProximityScore( void ) { return m_iRoundProximityScore ; }

	void	ClearScore( void ); 
	void	AddScore( int iPoints );

#define USE_OLD_SCORE_SYSTEM 0

#if ( USE_OLD_SCORE_SYSTEM )
	int		GetScore() const { return m_iScore; }
#else
	int		GetScore() const { return m_iContributionScore; }
#endif

	int	GetContributionScore( void ) { return m_iContributionScore; }
	void ClearContributionScore( void ){ m_iContributionScore = 0; pl.score = 0; }
	void AddContributionScore( int iPoints );

	uint32 GetHumanPlayerAccountID() const { return m_uiAccountId; }
	void SetHumanPlayerAccountID( uint32 uiAccountId );
	
	void    RemoveNemesisRelationships();
	void	SetDeathFlags( int iDeathFlags ) { m_iDeathFlags = iDeathFlags; }
	int		GetDeathFlags() { return m_iDeathFlags; }		
	int		GetNumBotsControlled( void ) { return m_botsControlled; }
	int		GetNumFootsteps( void ) { return m_iFootsteps; }
	int		GetMediumHealthKills( void ) { return m_iMediumHealthKills; }
	int		GetTotalCashSpent( void ) { return m_iTotalCashSpent; }
	int		GetCashSpentThisRound( void ) { return m_iCashSpentThisRound; }

	int		GetEndMatchNextMapVote( void ) { return m_nEndMatchNextMapVote; }

	void	ClearTRModeHEGrenade( void ) { m_bGunGameTRModeHasHEGrenade = false; }
	void	ClearTRModeFlashbang( void ) { m_bGunGameTRModeHasFlashbang = false; }
	void	ClearTRModeMolotov( void ) { m_bGunGameTRModeHasMolotov = false; }
	void	ClearTRModeIncendiary( void ) { m_bGunGameTRModeHasIncendiary = false; }

private:
	CNetworkArray( bool, m_bPlayerDominated, MAX_PLAYERS+1 );		// array of state per other player whether player is dominating other players
	CNetworkArray( bool, m_bPlayerDominatingMe, MAX_PLAYERS+1 );	// array of state per other player whether other players are dominating this player

	CNetworkArray( int, m_iWeaponPurchasesThisRound, MAX_WEAPONS );	// number of times weapons purchased this round; used to limit repurchases

	CNetworkVar( bool, m_bIsTaunting );
	CNetworkVar( bool, m_bIsThirdPersonTaunt );
	CNetworkVar( bool, m_bIsHoldingTaunt );
	CNetworkVar( float, m_flTauntYaw );

	CNetworkVar( bool, m_bIsLookingAtWeapon );
	CNetworkVar( bool, m_bIsHoldingLookAtWeapon );

	float m_flTauntEndTime;
	float m_flLookWeaponEndTime;
	bool m_bMustNotMoveDuringTaunt;

	// [menglish] number of rounds this player has caused to be won for their team
	int m_iMVPs;
	uint32 m_uiAccountId;

	// Competitive scorecard tracking information
	int m_iEnemyKills;
	int m_iEnemyKillHeadshots;
	int m_iEnemy3Ks;
	int m_iEnemy4Ks;
	int m_iEnemy5Ks;
	int m_iEnemyKillsAgg;
	int m_numFirstKills;
	int m_numClutchKills;
	int m_numPistolKills;
	int m_numSniperKills;

	// [pfreese] new contribution score system
	int m_iScore;
	int m_iRoundProximityScore;
	int m_iRoundContributionScore;
	int m_iContributionScore;

	// [dwenger] adding tracking for fun fact
	bool m_bWieldingKnifeAndKilledByGun;
	int m_botsControlled;
	int m_iFootsteps;
	int m_iMediumHealthKills;

	// tracking Money spent to calculate Cash Per Kill
	int m_iTotalCashSpent;
	// tracking just the cash spent in this round
	int m_iCashSpentThisRound;

	// this players vote for the next map in the mapgroup at the end of the match
	int m_nEndMatchNextMapVote;

	// [dkorus] achievement tracking
	bool m_wasKilledThisRound;
	int	 m_numRoundsSurvived;
	int  m_maxNumRoundsSurvived;


	// [dwenger] adding tracking for which weapons this player has used in a round
	CUtlVector<CSWeaponID> m_WeaponTypesUsed; 
	CUtlVector<CSWeaponID> m_WeaponTypesHeld;
	CUtlVector<CSWeaponID> m_WeaponTypesRunningOutOfAmmo; 
	CUtlVector<int>		   m_BurnDamageDeltVec;

	CBaseCombatWeapon* m_lastWeaponBeforeC4AutoSwitch;

	int m_iDeathFlags; // Flags holding revenge and domination info about a death

	// Track last damage type
	int	m_LastDamageType;

	// Used to track whether or not a player in TR gun game mode has earned grenades
	bool m_bGunGameTRModeHasHEGrenade;
	bool m_bGunGameTRModeHasFlashbang;
	bool m_bGunGameTRModeHasMolotov;
	bool m_bGunGameTRModeHasIncendiary;

public:
	bool IsAbleToInstantRespawn( void );
	bool IsAssassinationTarget( void ) const;
	char const * IsAbleToApplySpray( trace_t *ptr, Vector *pvecForward, Vector *pvecRight );

	uint32 GetActiveQuestID( void ) const;
	QuestProgress::Reason GetQuestProgressReason( void ) const;

private:
	CNetworkVar( bool, m_bIsAssassinationTarget );	// This player is an assassination target for an active mission

#if CS_CONTROLLABLE_BOTS_ENABLED
public: 

	bool CanControlBot( CCSBot *pBot ,bool bSkipTeamCheck = false );
	bool TakeControlOfBot( CCSBot *pBot, bool bSkipTeamCheck = false );
	void ReleaseControlOfBot( void );
	CCSBot* FindNearestControllableBot( bool bMustBeValidObserverTarget );
	bool IsControllingBot( void )							const { return m_bIsControllingBot; }

	bool HasControlledBot( void )						const { return m_hControlledBot.Get() != NULL; }
	CCSPlayer* GetControlledBot( void )				const { return static_cast<CCSPlayer*>(m_hControlledBot.Get()); }
	void SetControlledBot( CCSPlayer* pOther )	      { m_hControlledBot = pOther; }

	bool HasControlledByPlayer( void )					const { return m_hControlledByPlayer.Get() != NULL; }
	CCSPlayer* GetControlledByPlayer( void )				const { return static_cast<CCSPlayer*>(m_hControlledByPlayer.Get()); }
	void SetControlledByPlayer( CCSPlayer* pOther )	      { m_hControlledByPlayer = pOther; }

	bool HasBeenControlledThisRound( void ) { return m_bHasBeenControlledByPlayerThisRound; }
	bool HasControlledBotThisRound( void ) {return m_bHasControlledBotThisRound;}



private:
	CNetworkVar( bool, m_bIsControllingBot );	// Are we controlling a bot? 
	// Note that this can be TRUE even if GetControlledPlayer() returns NULL, 
	// IFF we started controlling a bot and then the bot was deleted for some reason. 

	CNetworkVar( bool, m_bCanControlObservedBot );	// set to true if we can take control of the bot we are observing, for client UI feedback. 

	CNetworkVar( int, m_iControlledBotEntIndex);	// Are we controlling a bot? 

	CHandle<CCSPlayer> m_hControlledBot;		// The is the OTHER player that THIS player is controlling
	CHandle<CCSPlayer> m_hControlledByPlayer;	// This is the OTHER player that is controlling THIS player
	bool m_bHasBeenControlledByPlayerThisRound;
	CNetworkVar( bool, m_bHasControlledBotThisRound );


	// Various values from this character before they took control or were controlled
	struct PreControlData
	{
		int m_iClass;		// CS class (such as CS_CLASS_PHOENIX_CONNNECTION or CS_CLASS_SEAL_TEAM_6)
		int m_iAccount;		// money
		int m_iAccountMoneyEarnedForNextRound; // money earned this round
		int m_iFrags;		// kills / score
		int m_iAssists;
		int m_iDeaths;		
	};

	void SavePreControlData();

	PreControlData	m_PreControlData;


public:
	PreControlData	GetBotPreControlData( void ) { return m_PreControlData; }

#endif // #if CS_CONTROLLABLE_BOTS_ENABLED

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
	CNetworkVarEmbedded( CAttributeContainerPlayer, m_AttributeManager );
#endif

	//----------------------------
	// ECONOMY INVENTORY MANAGEMENT
public:
	// IInventoryUpdateListener
	virtual void InventoryUpdated( CPlayerInventory *pInventory );
	virtual void SOCacheUnsubscribed( const CSteamID & steamIDOwner ) { /*m_Shared.SetLoadoutUnavailable( true );*/ }
	void		VerifySOCache();
#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

	void		UpdateInventory( bool bInit );

	// Inventory access
	CCSPlayerInventory	*Inventory( void ) { return &m_Inventory; }
	const CCSPlayerInventory	*Inventory( void ) const { return &m_Inventory; }
	CEconItemView *GetEquippedItemInLoadoutSlot( int iLoadoutSlot ) { return Inventory()->GetInventoryItemByItemID( m_EquippedLoadoutItemIndices[iLoadoutSlot] ); }
	CEconItemView *GetEquippedItemInLoadoutSlotOrBaseItem( int iLoadoutSlot );

	uint32 RecalculateCurrentEquipmentValue( void );
	void UpdateFreezetimeEndEquipmentValue( void );

	//void UpdateAppearanceIndex( void );

private:
	CCSPlayerInventory	m_Inventory;
	// Items that have been equipped on this player instance (the inventory loadout may have changed)
	itemid_t				m_EquippedLoadoutItemIndices[LOADOUT_POSITION_COUNT];

	//uint16 m_unAppearanceIndex;
	CNetworkVar( uint16, m_unCurrentEquipmentValue );
	CNetworkVar( uint16, m_unRoundStartEquipmentValue );
	CNetworkVar( uint16, m_unFreezetimeEndEquipmentValue );


private:
	// override for weapon driving animations
	bool UpdateLayerWeaponDispatch( CAnimationLayer *pLayer, int iSequence );
public:
	virtual float	GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence );

	bool GetBulletHitLocalBoneOffset( const trace_t &tr, int &boneIndexOut, Vector &vecPositionOut, QAngle &angAngleOut );

	// Quest state
private:
	// Can we make progress in our current quest?  If not, why not?
	CNetworkVar( QuestProgress::Reason, m_nQuestProgressReason );
};

inline CSPlayerState CCSPlayer::State_Get() const
{
	return m_iPlayerState;
}

inline CCSPlayer *ToCSPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CCSPlayer*>( pEntity );
}


inline bool CCSPlayer::IsReloading( void ) const
{
	CBaseCombatWeapon *gun = GetActiveWeapon();
	if (gun == NULL)
		return false;

	return gun->m_bInReload;
}

inline bool CCSPlayer::IsProtectedByShield( void ) const
{ 
	return HasShield() && IsShieldDrawn();
}

inline bool CCSPlayer::IsBlind( void ) const
{ 
	return gpGlobals->curtime < m_blindUntilTime;
}

inline bool CCSPlayer::IsBlindForAchievement()
{
	return (m_blindStartTime + m_flFlashDuration) > gpGlobals->curtime;
}

inline bool CCSPlayer::IsAutoFollowAllowed( void ) const		
{ 
	return (gpGlobals->curtime > m_allowAutoFollowTime); 
}

inline void CCSPlayer::InhibitAutoFollow( float duration )	
{ 
	m_allowAutoFollowTime = gpGlobals->curtime + duration; 
}

inline void CCSPlayer::AllowAutoFollow( void )	
{ 
	m_allowAutoFollowTime = 0.0f; 
}

inline int CCSPlayer::GetClass( void ) const
{
	return m_iClass;
}

inline int CCSPlayer::GetTeammatePreferredColor( void ) const
{
	return m_iTeammatePreferredColor;
}

inline const char *CCSPlayer::GetClanTag( void ) const
{
	return m_szClanTag;
}

inline const char *CCSPlayer::GetClanName( void ) const
{
	return m_szClanName;
}

#endif	//CS_PLAYER_H
