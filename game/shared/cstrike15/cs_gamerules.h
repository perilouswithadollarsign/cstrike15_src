//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef CS_GAMERULES_H
#define CS_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif


#include "teamplay_gamerules.h"
#include "convar.h"
#include "cs_shareddefs.h"
#include "cs_weapon_parse.h"
#include "gamevars_shared.h"
#include "bot/bot_constants.h"
#include "../../../common/input_device.h"
#include "cstrike15_gcconstants.h"
#include "cstrike15_gcmessages.pb.h"
#include "usermessages.h"
#if defined( GAME_DLL )
#include "maprules.h"
#endif

//#define DEBUG_QUESTS_IN_CLIENT


#ifdef CLIENT_DLL
	#include "networkstringtable_clientdll.h"
	class C_CSPlayer;

	#define CCSPlayer C_CSPlayer
#else
	#include "funfactmgr_cs.h"
	class CCSPlayer;
#endif

#define	WINNER_NONE		0
#define WINNER_DRAW		1
#define WINNER_TER		TEAM_TERRORIST
#define WINNER_CT		TEAM_CT	

#define CUSTOM_BOT_DIFFICULTY_NOBOTS	0
#define CUSTOM_BOT_DIFFICULTY_DUMB		1
#define CUSTOM_BOT_DIFFICULTY_EASY		2
#define CUSTOM_BOT_DIFFICULTY_MEDIUM	3
#define CUSTOM_BOT_DIFFICULTY_HARD		4
#define CUSTOM_BOT_DIFFICULTY_EXPERT	5

#define CUSTOM_BOT_MIN_DIFFICULTY_FOR_AWARDS_PROGRESS	CUSTOM_BOT_DIFFICULTY_EASY

#define MAX_WEAPON_NAME_POPUP_RANGE 128.0

#define MAX_GIFT_GIVERS_FEATURED_COUNT 4

#define MAX_TOURNAMENT_ACTIVE_CASTER_COUNT 4

class CCSBot;
class CHostage;
class CCSWeaponInfo;

struct quest_data_t;

extern ConVar mp_startmoney;
extern ConVar mp_maxmoney;
extern ConVar mp_afterroundmoney;
extern ConVar mp_playercashawards;
extern ConVar mp_teamcashawards;
extern ConVar mp_tkpunish;
extern ConVar mp_c4timer;
extern ConVar mp_buytime;
extern ConVar mp_freezetime;
extern ConVar mp_playerid;
extern ConVar mp_defuser_allocation;
extern ConVar mp_death_drop_gun;
extern ConVar mp_death_drop_grenade;
extern ConVar mp_death_drop_defuser;
extern ConVar ammo_grenade_limit_total;
extern ConVar sv_competitive_official_5v5;

namespace DefuserAllocation
{
	enum Type
	{
		None = 0,
		Random = 1,
		All = 2,
	};
};

namespace TeamCashAward
{
	enum Type
	{
		NONE = 0,
		TERRORIST_WIN_BOMB,
		ELIMINATION_HOSTAGE_MAP_T,
		ELIMINATION_HOSTAGE_MAP_CT,
		ELIMINATION_BOMB_MAP,
		WIN_BY_TIME_RUNNING_OUT_HOSTAGE,
		WIN_BY_TIME_RUNNING_OUT_BOMB,
		WIN_BY_DEFUSING_BOMB,
		WIN_BY_HOSTAGE_RESCUE,
		LOSER_BONUS,
		LOSER_BONUS_CONSECUTIVE_ROUNDS,
		RESCUED_HOSTAGE,
		HOSTAGE_ALIVE,
		PLANTED_BOMB_BUT_DEFUSED,
		HOSTAGE_INTERACTION,
		LOSER_ZERO,
		SURVIVE_GUARDIAN_WAVE,
		CUSTOM_AWARD,
	};
};


namespace RoundResult
{
	enum Reason
	{
		UNDEFINED = 0,
		CT_WIN_ELIMINATION,
		CT_WIN_RESCUE,
		CT_WIN_DEFUSE,
		CT_WIN_TIME,
		T_WIN_ELIMINATION,
		T_WIN_BOMB,
		T_WIN_TIME,

		COUNT,

		UNKNOWN
	};

};




namespace PlayerCashAward
{
	enum Type
	{
		NONE = 0,
		KILL_TEAMMATE,
		KILLED_ENEMY,
		BOMB_PLANTED,
		BOMB_DEFUSED,
		RESCUED_HOSTAGE,
		INTERACT_WITH_HOSTAGE,
		DAMAGE_HOSTAGE,
		KILL_HOSTAGE,
		RESPAWN,
		GET_KILLED,
	};
};

namespace AutobalanceStatus
{
	enum Type
	{
		NONE = 0,
		NEXT_ROUND,
		THIS_ROUND
	};
};

//--------------------------------------------------------------------------------------------------------------
struct GGWeaponAliasName
{
	CSWeaponID id;
	const char *aliasName;
};

#define GGLIST_PISTOLS_TOTAL		9
#define GGLIST_RIFLES_TOTAL			7
#define GGLIST_MGS_TOTAL			2
#define GGLIST_SGS_TOTAL			4
#define GGLIST_SMGS_TOTAL			6
#define GGLIST_SNIPERS_TOTAL		3

#define GGLIST_PISTOLS_START	0
#define GGLIST_PISTOLS_LAST		(GGLIST_PISTOLS_START+GGLIST_PISTOLS_TOTAL-1)
#define GGLIST_RIFLES_START		GGLIST_PISTOLS_LAST+1
#define GGLIST_RIFLES_LAST		(GGLIST_RIFLES_START+GGLIST_RIFLES_TOTAL-1)
#define GGLIST_MGS_START		GGLIST_RIFLES_LAST+1
#define GGLIST_MGS_LAST			(GGLIST_MGS_START+GGLIST_MGS_TOTAL-1)
#define GGLIST_SGS_START		GGLIST_MGS_LAST+1
#define GGLIST_SGS_LAST			(GGLIST_SGS_START+GGLIST_SGS_TOTAL-1)
#define GGLIST_SMGS_START		GGLIST_SGS_LAST+1
#define GGLIST_SMGS_LAST			(GGLIST_SMGS_START+GGLIST_SMGS_TOTAL-1)
#define GGLIST_SNIPERS_START		GGLIST_SMGS_LAST+1
#define GGLIST_SNIPERS_LAST			(GGLIST_SNIPERS_START+GGLIST_SNIPERS_TOTAL-1)

//--------------------------------------------------------------------------------------------------------------
// NOTE: Array must be NULL-terminated
static GGWeaponAliasName ggWeaponAliasNameList[] =
{
	//pistols
	{ WEAPON_DEAGLE, "deagle" },
	{ WEAPON_DEAGLE, "revolver" },
	{ WEAPON_ELITE, "elite" },
	{ WEAPON_FIVESEVEN, "fiveseven" },
	{ WEAPON_GLOCK, "glock" },
	{ WEAPON_TEC9, "tec9" },
	{ WEAPON_HKP2000, "hkp2000" },
	{ WEAPON_HKP2000, "usp_silencer" },
	{ WEAPON_P250, "p250" },

	//rifles
	{ WEAPON_AK47, "ak47" },
	{ WEAPON_AUG, "aug" },
	{ WEAPON_FAMAS, "famas" },
	{ WEAPON_GALILAR, "galilar" },
	{ WEAPON_M4A1, "m4a1" },
	{ WEAPON_M4A1, "m4a1_silencer" },
	{ WEAPON_SG556, "sg556" },

	//mgs
	{ WEAPON_M249, "m249" },
	{ WEAPON_NEGEV, "negev" },

	//shotguns
	{ WEAPON_XM1014, "xm1014" },
	{ WEAPON_MAG7, "mag7" },
	{ WEAPON_SAWEDOFF, "sawedoff" },
	{ WEAPON_NOVA, "nova" },

	//smgs
	{ WEAPON_MAC10, "mac10" },
	{ WEAPON_P90, "p90" },
	{ WEAPON_UMP45, "ump45" },
	{ WEAPON_BIZON, "bizon" },
	{ WEAPON_MP7, "mp7" },
	{ WEAPON_MP9, "mp9" },

	//snipers
	//	{ WEAPON_SSG08, "ssg08" },
	{ WEAPON_SCAR20, "scar20" },
	{ WEAPON_G3SG1, "g3sg1" },
	{ WEAPON_AWP, "awp" },

	{ WEAPON_NONE, "" }
};

class CEconItemPreviewDataBlock; // forward declare item data
class CCSUsrMsg_PlayerDecalDigitalSignature; // forward declare proto message

#ifndef CLIENT_DLL
	extern ConVar mp_autoteambalance;
#endif // !CLIENT_DLL


#ifdef CLIENT_DLL
	#define CCSGameRules C_CSGameRules
	#define CCSGameRulesProxy C_CSGameRulesProxy
#endif

#if !defined( CLIENT_DLL )

// forward declare GC message
class CMsgGCCStrike15_v2_MatchmakingServerRoundStats;
class CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve;

int ScramblePlayersSort( CCSPlayer* const *p1, CCSPlayer* const *p2 );

class CCSMatch
{		
public:
	CCSMatch();

	void Reset( void );

	void SetPhase( GamePhase phase );
	GamePhase GetPhase( void ) const { return m_phase; }

	//These functions add to both the score and the number of rounds
	void AddTerroristWins( int numWins );
	void AddCTWins( int numWins);
	void IncrementRound( int nNumRounds );

	//These functions only adjust the score (without adding to the number of rounds played)
	void AddTerroristBonusPoints( int numWins );
	void AddCTBonusPoints( int numWins);

	int GetTerroristScore( void ) const 	{ return m_terroristScoreTotal; }
	int GetCTScore( void ) const  			{ return m_ctScoreTotal; }	

	int GetTeamScore( int nTeam ) const		{ switch( nTeam ) { case TEAM_TERRORIST: return GetTerroristScore(); case TEAM_CT: return GetCTScore(); default: Assert( !"unexpected value for nTeam" ); return 0; } }
	int GetRoundsPlayed( void ) const		{ return m_actualRoundsPlayed; }	
	
	//Since the teams change in halftime modes and we want to retain their scores, we swap the scores
	//between halves.
	void SwapTeamScores( void );
	
	int GetWinningTeam( void );	

	//These are the internal functions that actually mess with the scores, adjusting the appropriate phase-specific scores as well.
	void AddTerroristScore( int score );
	void AddCTScore( int score );
	void GoToOvertime( int numOvertimesToAdd );

private:
	//This is called anytime the match-internal scores are updated to reflect the changes in the team object (so the scores can be replicated)
	void UpdateTeamScores( void );

	// Called when we wish to change the full all-talk rules, based on entering a specific phase of the match
	void EnableFullAlltalk( bool bEnable );

	//This is the number of rounds that have been played, regardless of the actual score of the match (e.g. Demolition mode can give bonus points)
	short m_actualRoundsPlayed;

	// This is the index of the overtime that is being played, 0 when game has no overtime or is still in regulation time, 1 for first overtime, 2 for second, etc.
	short m_nOvertimePlaying;

	short m_ctScoreFirstHalf;
	short m_ctScoreSecondHalf;
	short m_ctScoreOvertime;
	short m_ctScoreTotal;

	short m_terroristScoreFirstHalf;
	short m_terroristScoreSecondHalf;
	short m_terroristScoreOvertime;
	short m_terroristScoreTotal;

	GamePhase m_phase;
};

class SpawnPoint : public CServerOnlyPointEntity
{
	DECLARE_CLASS( SpawnPoint, CServerOnlyPointEntity );
	DECLARE_DATADESC();
public:
	SpawnPoint();
	void Spawn( void );
	bool IsEnabled() { return m_bEnabled; }
	void InputSetEnabled( inputdata_t &inputdata );
	void InputSetDisabled( inputdata_t &inputdata );
	void InputToggleEnabled( inputdata_t &inputdata );
	void SetSpawnEnabled( bool bEnabled );

	int		m_iPriority;
	bool	m_bEnabled;
	int		m_nType;

	enum Type
	{
		Default = 0,
		Deathmatch = 1,
		ArmsRace = 2,
	};
};

class SpawnPointCoopEnemy : public SpawnPoint
{
	DECLARE_CLASS( SpawnPointCoopEnemy, SpawnPoint );
	DECLARE_DATADESC();
public:	
	SpawnPointCoopEnemy();
	void	Spawn( void );
	void	Precache( void );
	const char *GetWeaponsToGive( void ) { return STRING( m_szWeaponsToGive ); }
	const char *GetPlayerModelToUse( void ) { return STRING( m_szPlayerModelToUse ); }
	int			GetArmorToSpawnWith( void ) { return m_nArmorToSpawnWith; }
	bool		ShouldStartAsleep( void ) { return m_bStartAsleep; }
	int			GetBotDifficulty( void ) { return m_nBotDifficulty; }
	bool		IsBotAgressive( void ) { return m_bIsAgressive; }
	CNavArea	*FindNearestArea( void );
	float HideRadius() const { return m_flHideRadius; }
	void HideRadius(bool val) { m_flHideRadius = val; }

	enum BotDefaultBehavior_t
	{
		DEFEND_AREA = 0,
		HUNT,
		CHARGE_ENEMY,
		DEFEND_INVESTIGATE,
	};
	BotDefaultBehavior_t GetDefaultBehavior() const { return m_nDefaultBehavior; }
	void SetDefaultBehavior( BotDefaultBehavior_t val ) { m_nDefaultBehavior = val; }

protected:
	string_t	m_szWeaponsToGive;
	string_t	m_szPlayerModelToUse;
	int			m_nArmorToSpawnWith;
	BotDefaultBehavior_t m_nDefaultBehavior;
	int			m_nBotDifficulty;
	bool		m_bIsAgressive;
	bool		m_bStartAsleep;
	float		m_flHideRadius;
	CNavArea	*m_pMyArea;
};

#endif  //!CLIENT_DLL

class CCSGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CCSGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CCSGameRules : public CTeamplayRules
{
public:
	DECLARE_CLASS( CCSGameRules, CTeamplayRules );

	// Stuff that is shared between client and server.
	bool IsFreezePeriod();
	bool IsWarmupPeriod() const;
	float GetWarmupPeriodEndTime() const;	
	bool IsWarmupPeriodPaused();
	void SetWarmupPeriodStartTime( float fl )	{ m_fWarmupPeriodStart = fl; }
	float GetWarmupPeriodStartTime( void )	{ return m_fWarmupPeriodStart; }
	bool AllowTaunts( void );

	bool IsTimeOutActive() const { return ( IsTerroristTimeOutActive() || IsCTTimeOutActive() ); }
	bool IsTerroristTimeOutActive() const { return m_bTerroristTimeOutActive; }
	bool IsCTTimeOutActive() const { return m_bCTTimeOutActive; }

	void StartTerroristTimeOut( void );
	void StartCTTimeOut( void );
	void EndTerroristTimeOut( void );
	void EndCTTimeOut( void );

	float GetCTTimeOutRemaining() const { return m_flCTTimeOutRemaining; }
	float GetTerroristTimeOutRemaining() const { return m_flTerroristTimeOutRemaining; }

	int GetCTTimeOuts( ) const { return m_nCTTimeOuts; }
	int GetTerroristTimeOuts( ) const { return m_nTerroristTimeOuts; }

#ifdef CLIENT_DLL
	virtual bool AllowThirdPersonCamera();
	bool IsGoodDownTime( void ); // this returns true when its a good time to do things like garbage collection
	bool IsLoadoutAllowed( void );
	void MarkClientStopRecordAtRoundEnd( bool bStop );
#endif

#ifndef CLIENT_DLL
	void StartWarmup( void );
	void EndWarmup( void );

	virtual bool IsTeamChangeSilent( CBasePlayer *pPlayer, int iTeamNum, bool bAutoTeam, bool bSilent ) { return bSilent || m_bForceTeamChangeSilent; }

	void CheckForGiftsLeaderboardUpdate();
#endif

	bool IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer );
				
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual float	GetRoundRestartTime( void ) { return m_flRestartRoundTime; }
	virtual bool	IsGameRestarting( void ) { return m_bGameRestart; }
	float GetMapRemainingTime();	// time till end of map, -1 if timelimit is disabled
	float GetMapElapsedTime();	// How much time has elapsed since the map started.
	float GetRoundRemainingTime() const;	// time till end of round
	float GetRoundStartTime();		// When this round started.
	float GetRoundElapsedTime();	// How much time has elapsed since the round started.
	float GetBuyTimeLength();
	int GetRoundLength() const { return m_iRoundTime; }
	int   SelectDefaultTeam( bool ignoreBots = false );
	int   GetHumanTeam();			// TEAM_UNASSIGNED if no restrictions

	void CalculateMaxGunGameProgressiveWeaponIndex( void );
	int GetMaxGunGameProgressiveWeaponIndex( void ) { return m_iMaxGunGameProgressiveWeaponIndex; }

	AcquireResult::Type IsWeaponAllowed( const CCSWeaponInfo *pWeaponInfo, int nTeamNumber ,CEconItemView *pItem = NULL  );

	bool IsBombDefuseMap() const;
	bool IsHostageRescueMap() const;
	bool MapHasBuyZone() const;
	bool CanSpendMoneyInMap();
	bool IsIntermission() const;
	bool IsLogoMap() const;
	bool IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer );
	bool IsSpawnPointHiddenFromOtherPlayers( CBaseEntity *pSpot, CBasePlayer *pPlayer, int nHideFromTeam = 0 );

	bool IsBuyTimeElapsed();
	bool IsMatchWaitingForResume( void );
	void SetMatchWaitingForResume( bool pause )				{ m_bMatchWaitingForResume = pause; };
	
	virtual int	DefaultFOV();

	// Get the view vectors for this mod.
	virtual const CViewVectors* GetViewVectors() const;

	bool IsCSGOBirthday( void );

	int  GetStartMoney( void );
	int	 GetMaxMoney( void );
	int  GetBetweenRoundMoney( void );
	bool PlayerCashAwardsEnabled( void );
	bool TeamCashAwardsEnabled( void );

	void AddHostageRescueTime( void );

	bool IsPlayingCustomGametype( void ) const;
	bool IsPlayingGunGameProgressive( void ) const;
	bool IsPlayingGunGameDeathmatch( void ) const;
	bool IsPlayingGunGameTRBomb( void ) const;
	bool IsPlayingGunGame( void ) const;
	bool IsPlayingClassic( void ) const;
	bool IsPlayingOffline( void ) const;
	bool IsPlayingTraining( void ) const;
	bool IsPlayingCooperativeGametype( void ) const;

	bool IsPlayingClassicCasual( void ) const;
	bool IsPlayingAnyCompetitiveStrictRuleset( void ) const;
	bool IsPlayingCoopGuardian( void ) const;
	bool IsPlayingCoopMission( void ) const;

	bool IsQueuedMatchmaking( void ) const;
	bool IsValveDS( void ) const;
	bool IsQuestEligible( void ) const;
	bool ShouldRecordMatchStats( void ) const;

	virtual bool IgnorePlayerKillCommand( void ) const { return IsQueuedMatchmaking() && !IsWarmupPeriod(); }
	
	bool IsAwardsProgressAllowedForBotDifficulty() const; // returns false if the user is playing offline with trivial bots (no bots, harmless bots)

	bool IsTeammateSolid( void ) const;				// returns true if teammates are solid obstacles in the current game mode
	bool IsArmorFree( void ) const;

	bool HasHalfTime( void ) const;

	bool IsRoundOver() const;

	int GetCustomBotDifficulty( void ) const;

	int GetCurrentGunGameWeapon ( int nCurrentWeaponIndex, int nTeamID );
	int GetNextGunGameWeapon( int nCurrentWeaponIndex, int nTeamID );
	int GetPreviousGunGameWeapon( int nCurrentWeaponIndex, int nTeamID );
	bool IsFinalGunGameProgressiveWeapon( int nCurrentWeaponIndex, int nTeamID );
	int GetGunGameNumKillsRequiredForWeapon( int nCurrentWeaponIndex, int nTeamID );

	void AddGunGameWeapon( const char* pWeaponName, int nNumKillsToUpgrade, int nTeamID );
	int GetNumProgressiveGunGameWeapons( int nTeamID ) const;
	int GetProgressiveGunGameWeapon( int nWeaponIndex, int nTeamID ) const { return nTeamID == TEAM_CT ? m_GGProgressiveWeaponOrderCT[nWeaponIndex] : m_GGProgressiveWeaponOrderT[nWeaponIndex]; }
	int GetProgressiveGunGameWeaponKillRequirement( int nWeaponIndex, int nTeamID ) const { return nTeamID == TEAM_CT ? m_GGProgressiveWeaponKillUpgradeOrderCT[nWeaponIndex] : m_GGProgressiveWeaponKillUpgradeOrderT[nWeaponIndex]; }
	int GetGunGameTRBonusGrenade( CCSPlayer *pPlayer );

	void IncrementGunGameTerroristWeapons( void );
	void IncrementGunGameCTWeapons( void );

	bool IsBotOnlyTeam( int nTeamNumber );
	int TeamCashAwardValue( TeamCashAward::Type reason);
	int PlayerCashAwardValue( PlayerCashAward::Type reason);

	int GetMaxSpectatorSlots( void ) const;

	float GetTimeUntilNextPhaseStarts( void ) 	{ return m_timeUntilNextPhaseStarts; }
	GamePhase GetGamePhase( void ) const		{ return ( GamePhase ) m_gamePhase.Get(); }
	int GetTotalRoundsPlayed( void ) const		{ return m_totalRoundsPlayed; }
	int GetOvertimePlaying( void ) const		{ return m_nOvertimePlaying; }

	int GetNumWinsToClinch( void ) const;
	bool IsLastRoundOfMatch() const;
	bool IsMatchPoint() const;

	// AreTeamsPlayingSwitchedSides() -- will return true when match is in second half, or in the half of overtime period where teams are switched.
	// Overtime logic is as follows: TeamA plays CTs as first half of regulation, then Ts as second half of regulation,
	//				then if tied in regulation continues to play Ts as first half of 1st overtime, then switches to CTs for second half of 1st overtime,
	//				then if still tied after 1st OT they continue to play CTs as first half of 2nd overtime, then switch to Ts for second half of 2nd overtime,
	//				then if still tied after 2nd OT they continue to play Ts as first half of 3rd overtime, then switch to CTs for second half of 3rd overtime,
	//				and so on until the match determines a winner.
	// So AreTeamsPlayingSwitchedSides will return true when TeamA is playing T-side and will return false when TeamA plays CT-side as they started match on CT
	// in scenario outlined above.
	bool AreTeamsPlayingSwitchedSides() const;

	void SetIsWarmupPeriod( bool bIsWarmup )	{ m_bWarmupPeriod = bIsWarmup; }

	bool HasMatchStarted()	{ return m_bHasMatchStarted; }

	int GetWeaponScoreForDeathmatch( int nPos );

	float GetRestartRoundTime( void ) const;

#if !defined( CLIENT_DLL )
	int GetCoopWaveNumber( void ) { return m_nGuardianModeWaveNumber; }
	CGameCoopMissionManager *GetCoopMissionManager( void );
	EHANDLE m_coopMissionManager;

	void SetCoopMissionManager( CBaseEntity *pPoint ) { m_coopMissionManager = pPoint; }

	void CoopSetBotQuotaAndRefreshSpawns( int nMaxEnemiesToSpawn );
	void CoopMissionSetNextRespawnIn( float flSeconds, bool bIncrementWaveNumber );
	void CoopMissionSpawnFirstEnemies( int nMaxEnemiesToSpawn );
	void CoopMissionSpawnNextWave( int nMaxEnemiesToSpawn );
	void CoopMissionRespawnDeadPlayers( void );
	void CoopCollectBonusCoin( void );

	virtual bool OnReplayPrompt( CBasePlayer *pVictim, CBasePlayer *pScorer ) OVERRIDE;
#endif
	int GetGuardianRequiredKills( void ) const;
	int GetGuardianKillsRemaining( void ) const;
	int GetGuardianSpecialWeapon( void ) const;

private:
	float GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pEntityToIgnore); // returns multiplier between 0.0 and 1.0 that is the percentage of any damage done from vecSrc to vecEnd that actually makes it.
	float GetAmountOfEntityVisible(Vector & src, CBaseEntity *player); // returns a value from 0 to 1 that is the percentage of player visible from src.

	CNetworkVar( bool, m_bFreezePeriod );	 // TRUE at beginning of round, set to FALSE when the period expires
	CNetworkVar( bool, m_bWarmupPeriod );	 // 
	CNetworkVar( float, m_fWarmupPeriodEnd ); // OBSOLETE. LEFT IN FOR DEMO COMPATIBILITY.
	CNetworkVar( float, m_fWarmupPeriodStart );

	CNetworkVar( bool, m_bTerroristTimeOutActive );
	CNetworkVar( bool, m_bCTTimeOutActive );
	CNetworkVar( float, m_flTerroristTimeOutRemaining );
	CNetworkVar( float, m_flCTTimeOutRemaining );
	CNetworkVar( int, m_nTerroristTimeOuts );
	CNetworkVar( int, m_nCTTimeOuts );

	CNetworkVar( bool, m_bMatchWaitingForResume ); // When mp_pause_match is called, this state becomes true and will prevent the next freezetime from ending.
	CNetworkVar( int, m_iRoundTime );		 // (From mp_roundtime) - How many seconds long this round is.	
	CNetworkVar( float, m_fMatchStartTime ); // time when match has started
	CNetworkVar( float, m_fRoundStartTime ); // time round has started		
	CNetworkVar( float, m_flRestartRoundTime ); // the global time when the round is supposed to end, if this is not 0
	CNetworkVar( bool, m_bGameRestart ); // True = mp_restartgame is being processed
	CNetworkVar( float, m_flGameStartTime );
	CNetworkVar( float, m_timeUntilNextPhaseStarts );
	CNetworkVar( int, m_gamePhase );
	CNetworkVar( int, m_totalRoundsPlayed);
	CNetworkVar( int, m_nOvertimePlaying);
	CNetworkVar( int, m_iHostagesRemaining );
	CNetworkVar( bool, m_bAnyHostageReached );
	CNetworkVar( bool, m_bMapHasBombTarget );
	CNetworkVar( bool, m_bMapHasRescueZone );
	CNetworkVar( bool, m_bMapHasBuyZone );	
	CNetworkVar( bool, m_bIsQueuedMatchmaking );	
	CNetworkVar( bool, m_bIsValveDS );
	CNetworkVar( bool, m_bLogoMap );		 // If there's an info_player_logo entity, then it's a logo map.
	CNetworkVar( int,  m_iNumGunGameProgressiveWeaponsCT );	// total number of CT gun game progressive weapons
	CNetworkVar( int,  m_iNumGunGameProgressiveWeaponsT );	// total number of T gun game progressive weapons
	CNetworkVar( int,  m_iSpectatorSlotCount );				// max spectator slots available				
	CNetworkArray( int, m_GGProgressiveWeaponOrderCT, 60 );				// CT gun game weapon order and # kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkArray( int, m_GGProgressiveWeaponOrderT, 60 );				// T gun game weapon order and # kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkArray( int, m_GGProgressiveWeaponKillUpgradeOrderCT, 60 );	// CT gun game number of kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkArray( int, m_GGProgressiveWeaponKillUpgradeOrderT, 60 );	// T gun game number of kills per weapon. Size is meant to be larger than the current number of different weapons defined in the CSWeaponID enum
	CNetworkVar( int, m_MatchDevice );
	CNetworkVar( bool, m_bHasMatchStarted );
	CNetworkVar( float, m_flDMBonusStartTime );
	CNetworkVar( float, m_flDMBonusTimeLength );
	CNetworkVar( uint16, m_unDMBonusWeaponLoadoutSlot );
	CNetworkVar( bool, m_bDMBonusActive );
	CNetworkVar( int, m_nNextMapInMapgroup );
	CNetworkString( m_szTournamentEventName, MAX_PATH );
	CNetworkString( m_szTournamentEventStage, MAX_PATH );
	CNetworkString( m_szMatchStatTxt, MAX_PATH );
	CNetworkString( m_szTournamentPredictionsTxt, MAX_PATH );
	CNetworkVar( int, m_nTournamentPredictionsPct );
	CNetworkVar( float, m_flCMMItemDropRevealStartTime );
	CNetworkVar( float, m_flCMMItemDropRevealEndTime );
	CNetworkVar( bool, m_bIsDroppingItems );	 // 
	CNetworkVar( bool, m_bIsQuestEligible );

	CNetworkVar( int, m_nGuardianModeWaveNumber );
	CNetworkVar( int, m_nGuardianModeSpecialKillsRemaining );
	CNetworkVar( int, m_nGuardianModeSpecialWeaponNeeded );	
	int m_nGuardianGrenadesToGiveBots;
public:

	// HACK: Low on time, don't have a better place for this. Hang some global data guardian needs to bookkeep heavy spawns.
	int m_nNumHeaviesToSpawn;
	//
	// Holiday gifts global presence
	//
	CNetworkVar( uint32, m_numGlobalGiftsGiven );
	CNetworkVar( uint32, m_numGlobalGifters );
	CNetworkVar( uint32, m_numGlobalGiftsPeriodSeconds );
	CNetworkArray( uint32, m_arrFeaturedGiftersAccounts, MAX_GIFT_GIVERS_FEATURED_COUNT );
	CNetworkArray( uint32, m_arrFeaturedGiftersGifts, MAX_GIFT_GIVERS_FEATURED_COUNT );

#define MAX_PROHIBITED_ITEMS 100
	CNetworkArray( uint16, m_arrProhibitedItemIndices, MAX_PROHIBITED_ITEMS );

	//
	// Tournament Casters
	//
	CNetworkArray( uint32, m_arrTournamentActiveCasterAccounts, MAX_TOURNAMENT_ACTIVE_CASTER_COUNT );

	
	// These three are part of a hack to move an expensive network call (ClientPrint) out of 
	// the death code and defer it by a short time. The level of network traffic is causing hitches on ps3.
// 	CCallQueue m_DeferredCallQueue;
// 	float m_flDeferredCallDispatchTime;

	// Tournament best-of-N
	CNetworkVar( int, m_numBestOfMaps );

	// halloween mask seed
	CNetworkVar( int, m_nHalloweenMaskListSeed );

	CNetworkVar( bool,  m_bBombDropped );
	CNetworkVar( bool,	m_bBombPlanted );
	CNetworkVar( int, m_iRoundWinStatus );	// 1 == CT's won last round, 2 == Terrorists did, 3 == Draw, no winner	
	CNetworkVar( int, m_eRoundWinReason );	// see: e_RoundEndReason
	CNetworkVar( bool, m_bTCantBuy );			// Who can and can't buy.
	CNetworkVar( bool, m_bCTCantBuy );
	CNetworkVar( float, m_flGuardianBuyUntilTime );

	CNetworkArray( int, m_iMatchStats_RoundResults, MAX_MATCH_STATS_ROUNDS );	
	CNetworkArray( int, m_iMatchStats_PlayersAlive_CT, MAX_MATCH_STATS_ROUNDS );
	CNetworkArray( int, m_iMatchStats_PlayersAlive_T, MAX_MATCH_STATS_ROUNDS );

	CNetworkArray( float,		m_TeamRespawnWaveTimes, MAX_TEAMS );	// Time between each team's respawn wave

	CEconQuestDefinition* GetActiveAssassinationQuest( void ) const;
	int GetActiveServerQuestID( void ) const { return m_iActiveAssassinationTargetMissionID; }
protected:
	CNetworkArray( float,		m_flNextRespawnWave, MAX_TEAMS );		// Minor waste, but cleaner code
	CNetworkVar( int, m_iActiveAssassinationTargetMissionID );			// we cannot change the name of this field for networking compatibility, but in coopgametypes this means the server questid
	bool m_bDontIncrementCoopWave;

public:
	float GetCMMItemDropRevealDuration();
	float GetCMMItemDropRevealEndTime() { return m_flCMMItemDropRevealEndTime; }
	bool IsDroppingItems() { return m_bIsDroppingItems; }

	loadout_positions_t GetDMBonusWeaponLoadoutSlot( void ) { return ( loadout_positions_t )m_unDMBonusWeaponLoadoutSlot.Get(); }
	float GetDMBonusStartTime( void ) { return m_flDMBonusStartTime; }
	float GetDMBonusTimeLength( void ) { return m_flDMBonusTimeLength; }
	bool IsDMBonusActive( void ) { return m_bDMBonusActive; }
	void SetNextMapInMapGroup( int nIndex ) { m_nNextMapInMapgroup = nIndex; }
	int GetNextMapInMapGroup( void ) { return m_nNextMapInMapgroup; }

#if defined( GAME_DLL )
	bool CheckGotGuardianModeSpecialKill( CWeaponCSBase* pAttackerWeapon );
#endif

	int GetNumHostagesRemaining( void ) { return m_iHostagesRemaining; }

	virtual CBaseCombatWeapon *GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon );

	virtual const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"d7NSuLq2"; } // both the client and server need this key

	void CreateFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize  );

	// This is unlocalized and shouldn't be used for display
	const char *GetDefaultTeamName( int nTeam );

	void OpenBuyMenu( int nPlayerIndex );
	void CloseBuyMenu( int nPlayerIndex );

	// respawn
	void			SetNextTeamRespawnWaveDelay( int iTeam, float flDelay );
	virtual float	GetNextRespawnWave( int iTeam, CBasePlayer *pPlayer );
	float			GetMinTimeWhenPlayerMaySpawn( CBasePlayer *pPlayer );
	float			GetRespawnWaveMaxLength( int iTeam, bool bScaleWithNumPlayers = true );
	float			GetRespawnTimeScalar( int iTeam );

	//float			GetTimeUntilEndMatchNextMapVoteEnds( void );
	bool			IsEndMatchVotingForNextMap();
	bool			IsEndMatchVotingForNextMapEnabled();

	// End Match Voting
	CNetworkArray( int, m_nEndMatchMapGroupVoteOptions, MAX_ENDMATCH_VOTE_PANELS );				// For mapgroups >10 maps these will be vote options

	// these functions cover recording and sending item drops for display in game modes where you don't allow drops during the match/round
	CUtlVector< CEconItemPreviewDataBlock * > m_ItemsPtrDroppedDuringMatch;
	const CUtlVector< CEconItemPreviewDataBlock * >& GetItemsDroppedDuringMatch( void ) const
	{
		return m_ItemsPtrDroppedDuringMatch;
	}
	void ClearItemsDroppedDuringMatch( void );
	void RecordPlayerItemDrop( const CEconItemPreviewDataBlock &iteminfo );

	static int GetMaxPlayers(); // always available

	// COOP
	void CoopResetRoundStartTime( void );
	void CoopGiveC4sToCTs( int nC4sToGive );

#ifndef CLIENT_DLL
	int m_coopBonusCoinsFound;
	bool m_coopBonusPistolsOnly;
	bool m_coopPlayersInDeploymentZone;
#endif

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
	CCSGameRules();
	~CCSGameRules();

	// helper function - this will eventually support side car info (and will probably move)
	const wchar_t* GetFriendlyMapName( const char* szShortName );
	bool GetFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize  );

	char const * GetTournamentEventName() const;
	char const * GetTournamentEventStage() const;
	char const * GetTournamentPredictionsTxt() const { return m_szTournamentPredictionsTxt; }
	int GetTournamentPredictionsPct() const { return m_nTournamentPredictionsPct; }
	char const * GetMatchStatTeamsTxt() const { return m_szMatchStatTxt; }

	CUserMessageBinder m_UMCMsgSendPlayerItemDrops;
	CUserMessageBinder m_UMCMsgSendPlayerItemFound;
	bool m_bMarkClientStopRecordAtRoundEnd;

	static void RecvProxy_TournamentActiveCasterAccounts( const CRecvProxyData *pData, void *pStruct, void *pOut );
	void ResetCasterConvars( void );
#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
	
	CCSGameRules();
	virtual ~CCSGameRules();

	virtual void RefreshSkillData( bool forceUpdate );

	void DumpTimers( void ) const;	// debugging to help track down a stuck server (rare?)

	CBaseEntity *GetPlayerSpawnSpot( CBasePlayer *pPlayer );

	static void EndRound();

	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void Think();

	void SwitchTeamsAtRoundReset( void );

	void ClearGunGameData( void );	

	void FreezePlayers( void );

	// Called at the end of GameFrame (i.e. after all game logic has run this frame)
	virtual void EndGameFrame( void );

	// Called when game rules are destroyed by CWorld
	virtual void LevelShutdown( void );

	void UpdateTeamClanNames( int nTeam );
	void UpdateTeamPredictions();

	// vscript function
	void	SetPlayerCompletedTraining( bool bCompleted );
	bool	GetPlayerCompletedTraining( void );
	void	SetBestTrainingCourseTime( int nTime );
	int		GetBestTrainingCourseTime( void );
	int		GetValveTrainingCourseTime( void );
	bool	IsLocalPlayerUsingController( void );
	void	TrainingGivePlayerAmmo( void );
	void	TrainingSetRadarHidden( bool bHide );
	void	TrainingSetMiniScoreHidden(  bool bHide );
	void	TrainingHighlightAmmoCounter( void );
	void	TrainingShowFinishMsgBox( void );
	void	TrainingShowExitDoorMsg( void );

	virtual void RegisterScriptFunctions( void );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
			void ShowSpawnPoints( int duration );

	virtual void SpawningLatePlayer( CCSPlayer* pLatePlayer );

	bool IsPistolRound( void );

	void HostageKilled( void ) { m_hostageWasKilled = true; }
	void HostageInjured( void ) { m_hostageWasInjured = true; }

	bool WasHostageKilled( void ) { return m_hostageWasKilled; }
	bool WasHostageInjured( void ) { return m_hostageWasInjured; }

	void PlayerTookDamage( CCSPlayer* player, const CTakeDamageInfo &damageInfo );

	void SendKickBanToGC( CCSPlayer *pPlayer, EMsgGCCStrike15_v2_MatchmakingKickBanReason_t eReason );
	void SendKickBanToGCforAccountId( uint32 uiAccountId, EMsgGCCStrike15_v2_MatchmakingKickBanReason_t eReason );

	virtual bool PlayTextureSounds( void ) { return true; }
	// Let the game rules specify if fall death should fade screen to black
	virtual bool  FlPlayerFallDeathDoesScreenFade( CBasePlayer *pl ) { return FALSE; }

	virtual void  RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );
	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld );

	virtual void UpdateClientData( CBasePlayer *pl );
	
	virtual CCSPlayer* CheckAndAwardAssists( CCSPlayer* pCSVictim, CCSPlayer* pKiller );

	// Death notices
	virtual void		DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	IGameEvent * CreateWeaponKillGameEvent( char const *szEventName, const CTakeDamageInfo &info );

	virtual void			InitDefaultAIRelationships( void );

	virtual const char *GetGameDescription( void ) { return "Counter-Strike: Global Offensive"; }  // this is the game name that gets seen in the server browser
	virtual const char *AIClassText(int classType);

	virtual bool FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon );

	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );

	// Called before entities are created
	virtual void LevelInitPreEntity();

	// Called after the map has finished loading.
	virtual void LevelInitPostEntity();

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	virtual bool ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual void ClientDisconnected( edict_t *pClient );

	virtual void ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );

	virtual int GetMaxHumanPlayers() const;

	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.
	// Also get rid of all world decals.
	void CleanUpMap();

	void CheckFreezePeriodExpired();
	void CheckRoundTimeExpired();

	// check if the scenario has been won/lost
	// return true if the scenario is over, false if the scenario is still in progress
	bool CheckWinConditions( void );	

	// this is used by entities like X and increments the round and ends it
	void IncrementAndTerminateRound( float tmDelay, int reason );

	void TerminateRound( float tmDelay, int reason );

	void ProcessEndOfRoundAchievements( int iWinnerTeam, int iReason );

	void SaveRoundDataInformation( char const *szFilenameOverride = NULL );
	void LoadRoundDataInformation( char const *szFilename );

	void ResetMasterSpawnPointsForCoop( void );
	// The following round-related functions are called as follows:
	//
	// At Match Start:
	//		PreRestartRound() -> RestartRound() -> PostRestartRound()
	//
	// During Subsequent Round Gameplay:
	//		RoundWin() is called at the point when the winner of the round has been determined - prior to free-play commencing
	//		PreRestartRound() is called with 1 second remaining prior to the round officially ending (This is after a round
	//						  winner has been chosen and players are allowed to continue playing)
	//		RoundEnd() is then called when the round has completely ended
	//		RestartRound() is then called immediately after RoundEnd()
	//		PostRestartRound() is called immediately after RestartRound() has completed
	void PreRestartRound( void );
	void RestartRound( void );
	void PostRestartRound( void );
	void RoundWin( void );
	void RoundEnd( void );
	int	 GetRoundsPlayed( void ) const		{ return m_match.GetRoundsPlayed(); }	

	void BalanceTeams( void );
	void HandleScrambleTeams( void );
	void HandleSwapTeams( void );
	void MoveHumansToHumanTeam( void );
	bool TeamFull( int team_id );
	int	 MaxNumPlayersOnTerrTeam();
	int  MaxNumPlayersOnCTTeam();

	bool WillTeamHaveRoomForPlayer( CCSPlayer* pPlayer, int newTeam );

	bool TeamStacked( int newTeam_id, int curTeam_id  );
	bool FPlayerCanRespawn( CBasePlayer *pPlayer );
	void UpdateTeamScores();
	void CheckMapConditions();
	void MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team);

	// Check various conditions to end the map.
	bool CheckGameOver();	
	bool CheckWinLimit();
	bool CheckFragLimit();

	void CheckLevelInitialized();
	void CheckRestartRound();

	virtual bool FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );

	bool CanPlayerHearTalker( CBasePlayer* pListener, CBasePlayer *pSpeaker, bool bTeamOnly );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly );

	void GuardianUpdateBotAccountAndWeapons( CCSPlayer *pBot );
	void GiveGuardianBotGrenades( CCSPlayer *pBot );
	
	// repsawning
	void SetTeamRespawnWaveTime( int iTeam, float flValue ); 
	virtual bool HasPassedMinRespawnTime( CBasePlayer *pPlayer );
	void	CheckRespawnWaves( void );


	// Checks if it still needs players to start a round, or if it has enough players to start rounds.
	// Starts a round and returns true if there are enough players.
	bool NeededPlayersCheck( bool &bNeededPlayers );

	// Setup counts for m_iNumTerrorist, m_iNumCT, m_iNumSpawnableTerrorist, m_iNumSpawnableCT, etc.
	void InitializePlayerCounts(
		int &NumAliveTerrorist,
		int &NumAliveCT,
		int &NumDeadTerrorist,
		int &NumDeadCT
		);

	// Check to see if the round is over for the various game types. Terminates the round
	// and returns true if the round should end.
	bool PrisonRoundEndCheck();
	bool BombRoundEndCheck( bool bNeededPlayers );
	bool HostageRescueRoundEndCheck( bool bNeededPlayers );
	bool BombPlantedRoundEndCheck();
	bool CTsReachedHostageRoundEndCheck();
	bool GuardianAllKillsAchievedCheck();
	bool m_bHasHostageBeenTouched;

	CCSPlayer* CalculateEndOfRoundMVP();

	// Check to see if the teams exterminated each other. Ends the round and returns true if so.
	bool TeamExterminationCheck(
		int NumAliveTerrorist,
		int NumAliveCT,
		int NumDeadTerrorist,
		int NumDeadCT,
		bool bNeededPlayers
		);

	void ReadMultiplayCvars();
	void SwapAllPlayers();

	void OnTeamsSwappedAtRoundReset();

	void ResetForTradeshow( void );		// reset player scores, reset team scores, reset player controls, restart round; used for demos

	void BroadcastSound( const char *sound, int team = -1 );


	// GUN GAME PROGRESSIVE FUNCTION
	bool GunGameProgressiveEndCheck( void );


	// BOMB MAP FUNCTIONS
	void GiveC4ToRandomPlayer();
	void GiveDefuserToRandomPlayer();
	CCSPlayer *IsThereABomber();
	bool IsThereABomb();

	// HOSTAGE MAP FUNCTIONS
	void HostageTouched();

	// Contribution score helpers
	void ScorePlayerKill( CCSPlayer* pPlayer );
	void ScorePlayerAssist( CCSPlayer* pPlayer, CCSPlayer* pCSVictim );
	void ScorePlayerObjectiveKill( CCSPlayer* pPlayer );
	void ScorePlayerSuicide( CCSPlayer* pPlayer );
	void ScorePlayerTeamKill( CCSPlayer* pPlayer );
	void ScorePlayerDamage( CCSPlayer* pPlayer, float fDamage );
	void ScoreBombPlant( CCSPlayer* pPlayer );
	void ScoreBombExploded( CCSPlayer* pPlayer );
	void ScoreBombDefuse( CCSPlayer* pPlayer, bool bMajorEvent );
	void ScoreHostageRescue( CCSPlayer* pPlayer, CHostage* pHostage, bool bMajorEvent );
	void ScoreHostageKilled( CCSPlayer* pPlayer );
	void ScoreHostageDamage( CCSPlayer* pPlayer, float fDamage );
	void ScoreFriendlyFire( CCSPlayer* pPlayer, float fDamage );
	void ScoreBlindEnemy( CCSPlayer* pPlayer );
	void ScoreBlindFriendly( CCSPlayer* pPlayer );
	
	// Sets up g_pPlayerResource.
	virtual void CreateStandardEntities();
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer );
	void ClientSettingsChanged( CBasePlayer *pPlayer );

	virtual bool CanClientCustomizeOwnIdentity() OVERRIDE;
	
	bool IsCareer( void ) const		{ return false; }		// returns true if this is a CZ "career" game

	virtual bool FAllowNPCs( void );	

	bool CheckSetVoteTime();

	void GoToMatchRestartIntermission();

	CCSMatch* GetMatch( void );
			   
	// Let's the match store recplicated vars in the game rules.
	void SetGamePhase( GamePhase newPhase ) { m_gamePhase = newPhase; }
	void SetTotalRoundsPlayed( int roundsPlayed ) { m_totalRoundsPlayed = roundsPlayed; }
	void SetOvertimePlaying( int nOvertimePlaying ) { m_nOvertimePlaying = nOvertimePlaying; }

	void SetScrambleTeamsOnRestart( bool scramble ) { m_bScrambleTeamsOnRestart = scramble; }
	bool GetScrambleTeamsOnRestart( void ) { return m_bScrambleTeamsOnRestart; }

	void SetSwapTeamsOnRestart( bool swapTeams ) { m_bSwapTeamsOnRestart = swapTeams; }
	bool GetSwapTeamsOnRestart( void ) { return m_bSwapTeamsOnRestart; }

	bool GameModeSupportsHealthBuffer( void );

protected:
	// these functions cover recording and sending item drops for display in game modes where you don't allow drops during the match/round
	void SendPlayerItemDropsToClient( void );
	bool m_bPlayerItemsHaveBeenDisplayed;

	void RewardMatchEndDrops( bool bAbortedMatch );
	virtual void GoToIntermission( bool bAbortedMatch = false );

	void UpdateMatchStats( CCSPlayer* pPlayer, int winnerIndex ) ;

	static void SplitScoreAmongPlayersInZone( int iPoints, int iTeam, CCSPlayer* pExcludePlayer, uint iPlace );
	static void SplitScoreAmongPlayersInRange( int iPoints, int iTeam, CCSPlayer* pExcludePlayer, const Vector& center, float fRangeInner, float fRangeOuter );

	void GotoTRBombModeHalftime( void );
	void EndTRBombModeHalftime( void );

	float CalculateAveragePlayerContributionScore( void );
	float CalculateAverageBotContributionScore( void );

public:
	void ModifyRealtimeBotDifficulty( CCSPlayer* pOnlyBotToProcess = NULL );

public:

	bool IsFriendlyFireOn() const;

	bool	IsLastRoundBeforeHalfTime( void );

	virtual void	SetAllowWeaponSwitch( bool allow );
	virtual bool	GetAllowWeaponSwitch( void );

	bool			ShouldGunGameSpawnBomb( void );
	void			SetGunGameSpawnBomb( bool allow );

	bool			IsClanTeam( CTeam *pTeam );

	//int				GetEndMatchCurrentMapVotes();

	// VARIABLES FOR ALL TYPES OF MAPS
	bool m_bLevelInitialized;	
	int m_iTotalRoundsPlayed;
	int m_iUnBalancedRounds;	// keeps track of the # of consecutive rounds that have gone by where one team outnumbers the other team by more than 2
	bool m_endMatchOnRoundReset;
	bool m_endMatchOnThink;
	
	float m_flCoopRespawnAndHealTime;

	// GAME TIMES
	int m_iFreezeTime;		// (From mp_freezetime) - How many seconds long the intro round (when players are frozen) is.

	int m_iNumTerrorist;		// The number of terrorists on the team (this is generated at the end of a round)
	int m_iNumCT;				// The number of CTs on the team (this is generated at the end of a round)
	int m_iNumSpawnableTerrorist;
	int m_iNumSpawnableCT;
	CUtlVector< int > m_arrSelectedHostageSpawnIndices; // The indices of hostage spawn locations selected for the match

	bool m_bFirstConnected;
	bool m_bCompleteReset;		// Set to TRUE to have the scores reset next time round restarts
	bool m_bPickNewTeamsOnReset;
	bool m_bScrambleTeamsOnRestart;
	bool m_bSwapTeamsOnRestart;

	enum EEndMatchMapVoteState_t
	{
		k_EEndMatchMapVoteState_MatchInProgress,
		k_EEndMatchMapVoteState_VoteInProgress,
		k_EEndMatchMapVoteState_VoteTimeEnded,
		k_EEndMatchMapVoteState_AllPlayersVoted,
		k_EEndMatchMapVoteState_SelectingWinner,
		k_EEndMatchMapVoteState_SettingNextLevel,
		k_EEndMatchMapVoteState_VoteAllDone,
	};

#ifndef CLIENT_DLL
	EEndMatchMapVoteState_t m_eEndMatchMapVoteState;
	int m_nEndMatchMapVoteWinner;
	CUtlVector<int>	m_nEndMatchTiedVotes;
	void CreateEndMatchMapGroupVoteOptions( void );

	enum EQueuedMatchmakingRematchState_t
	{
		k_EQueuedMatchmakingRematchState_MatchInProgress,
		k_EQueuedMatchmakingRematchState_VoteStarting,
		k_EQueuedMatchmakingRematchState_VoteToRematchInProgress,
		k_EQueuedMatchmakingRematchState_VoteToRematchSucceeded,
		k_EQueuedMatchmakingRematchState_VoteToRematchFailed,
		k_EQueuedMatchmakingRematchState_VoteToRematchFailed_Done,
		k_EQueuedMatchmakingRematchState_VoteToRematch_Done,
		k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender,
		k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender,
		k_EQueuedMatchmakingRematchState_VoteToRematch_Aborted,
	};
	EQueuedMatchmakingRematchState_t m_eQueuedMatchmakingRematchState;
	bool m_bNeedToAskPlayersForContinueVote;
	uint32 m_numQueuedMatchmakingAccounts;
	char *m_pQueuedMatchmakingReservationString;
	uint32 m_numTotalTournamentDrops;
	uint32 m_numSpectatorsCountMax;
	uint32 m_numSpectatorsCountMaxTV;
	uint32 m_numSpectatorsCountMaxLnk;
	CMsgGCCStrike15_v2_MatchmakingServerRoundStats *m_pQueuedMatchmakingReportedRoundStats;
	static CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve sm_QueuedServerReservation;
	void ReportRoundEndStatsToGC( CMsgGCCStrike15_v2_MatchmakingServerRoundStats **ppAllocateStats = NULL );

	struct CQMMPlayerData_t
	{
		CQMMPlayerData_t()
		{
			Q_memset( this, 0, offsetof( CQMMPlayerData_t, m_uiCustomNonPodFields ) );
		}
		void Reset()
		{	// Reset everything starting from m_numKills to the end
			Q_memset( ( ( char * ) this ) + offsetof( CQMMPlayerData_t, m_numKills ), 0,
				offsetof( CQMMPlayerData_t, m_uiCustomNonPodFields ) - offsetof( CQMMPlayerData_t, m_numKills ) );
		}

		uint32 m_uiPlayerAccountId;		// QMM player account ID
		int m_iDraftIndex;				// Index of the player in the draft [0-4: team 1; 5-9: team 2]
		uint32 m_msDisconnectionTimestamp; // Timestamp of player's disconnection if not 0
		uint32 m_uiAbandonRecordedReason; // We recorded the fact that the player abandoned the match and this is the reason
		bool m_bAbandonAllowsSurrender;	// This player abandoning the match allows for his teammates to surrender
		char m_chPlayerName[128];		// Last known player name
		bool m_bEverFullyConnected;			// Has this player ever connected to the server
		bool m_bDisconnection1MinWarningPrinted;	// Whether 1 minute warning has been printed

		// RESETTABLE SECTION:
		int m_numKills;					// number of kills
		int m_numAssists;				// number of assists
		int m_numDeaths;				// number of deaths
		int m_numScorePoints;			// number of score points
		int m_numMVPs;					// number of MVP rounds
		int m_cash;						// player's cash
		int m_numTeamKills;				// number of team kills
		int m_numTeamDamagePoints;		// number of team damage points
		int m_numHostageKills;			// number of hostages killed
		int m_numEnemyKills;			// number of enemies killed
		int m_numEnemyKillHeadshots;	// number of enemies killed with headshot
		int m_numEnemy3Ks;				// number of 3Ks
		int m_numEnemy4Ks;				// number of 4Ks
		int m_numEnemy5Ks;				// number of 5Ks
		int m_numEnemyKillsAgg;			// number of enemies killed on aggregate
		int m_numRoundsWon;				// rounds won
		int m_numFirstKills;			// number of times this player got first kill of the round
		int m_numClutchKills;			// number of times this player got a kill with no teammates alive
		int m_numPistolKills;			// number of pistol kills this player got
		int m_numSniperKills;			// number of sniper kills this player got
		int m_numHealthPointsRemovedTotal;	// total number of health points removed (used for coop gameplay)
		int m_numHealthPointsDealtTotal;	// total number of health points dmg dealt to enemies (used for coop gameplay)
		int m_numShotsFiredTotal;			// total number of shots fired (incremented by one every time player pulls trigger, not per each shotgun pellet; used for coop gameplay)
		int m_numShotsOnTargetTotal;		// total number of shots on target (incremented by no moer than one per each time player pulls trigger, not per each shotgun pellet; used for coop gameplay)

		bool m_bReceiveNoMoneyNextRound;	// player is marked to not receive any money next round

		// match stat data for the spectator graphs
		int m_iMatchStats_Kills[MAX_MATCH_STATS_ROUNDS];		
		int m_iMatchStats_Damage[MAX_MATCH_STATS_ROUNDS];	
		int m_iMatchStats_EquipmentValue[MAX_MATCH_STATS_ROUNDS];	
		int m_iMatchStats_MoneySaved[MAX_MATCH_STATS_ROUNDS];	
		int m_iMatchStats_KillReward[MAX_MATCH_STATS_ROUNDS];
		int m_iMatchStats_LiveTime[MAX_MATCH_STATS_ROUNDS];
		int m_iMatchStats_Deaths[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_Assists[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_HeadShotKills[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_Objective[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_CashEarned[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_UtilityDamage[ MAX_MATCH_STATS_ROUNDS ];
		int m_iMatchStats_EnemiesFlashed[ MAX_MATCH_STATS_ROUNDS ];

		// per weapon kills
		uint32 m_uiCustomNonPodFields;

		// END RESETTABLE SECTION

		CUtlMap< uint32, uint32, int, CDefLess< uint32 > > m_mapQuestEventPoints;

		// TODO: If we have more attributes we want to be processed with the timed rewards job, try to generify this
		typedef CUtlMap< itemid_t, attrib_value_t, int, CDefLess< itemid_t > > StattrakMusicKitValues_t;
		StattrakMusicKitValues_t m_mapMusicKitUpdates;

		// once the player purchases an econ item, cache it and rebuy it from the cache.
		typedef CUtlMap< uint16, item_definition_index_t, int16, CDefLess< uint16 > > LoadoutSlotToDefIndexMap_t;
		LoadoutSlotToDefIndexMap_t m_mapLoadoutSlotToItem[ 2 ];

	};
	typedef CUtlMap< uint32, CQMMPlayerData_t *, int32, CDefLess< uint32 > > QueuedMatchmakingPlayersDataMap_t;
	QueuedMatchmakingPlayersDataMap_t m_mapQueuedMatchmakingPlayersData;
	CQMMPlayerData_t * QueuedMatchmakingPlayersDataFind( uint32 uiAccountID ) const
	{
		if ( !uiAccountID ) return NULL;
		QueuedMatchmakingPlayersDataMap_t::IndexType_t idx = m_mapQueuedMatchmakingPlayersData.Find( uiAccountID );
		return ( idx == m_mapQueuedMatchmakingPlayersData.InvalidIndex() ) ? NULL : m_mapQueuedMatchmakingPlayersData.Element( idx );
	}
	CQMMPlayerData_t * QueuedMatchmakingPlayersDataFindOrCreate( CCSPlayer *pPlayer );

	struct CGcBanInformation_t
	{
		uint32 m_uiReason;
		double m_dblExpiration;
	};
	typedef CUtlMap< uint32, CGcBanInformation_t > GcBanInformationMap_t;
	static GcBanInformationMap_t sm_mapGcBanInformation;

	bool m_bForceTeamChangeSilent;
	bool m_bLoadingRoundBackupData;

	class ICalculateEndOfRoundMVPHook_t
	{
	public:
		virtual CCSPlayer* CalculateEndOfRoundMVP() = 0;
	};
	ICalculateEndOfRoundMVPHook_t *m_pfnCalculateEndOfRoundMVPHook;

	typedef CUtlMap< int32, uint32, int32, CDefLess< int32 > > MapMatchInfoShownCounts;
	MapMatchInfoShownCounts m_mapMatchInfoShownCounts; // tracks how many times each match info piece was shown
	enum MapMatchInfoShownCountsSpecial_t
	{
		k_MapMatchInfoShownCounts_None = 0xF0000,
		k_MapMatchInfoShownCounts_Predictions = 0xF0001,
		k_MapMatchInfoShownCounts_MapDraft = 0xF0002,
	};
	int m_nMatchInfoShowType; // which match info is preferred to be shown
	float m_flMatchInfoDecidedTime; // what curtime moment was when match info was decided

	struct ServerPlayerDecalData_t
	{
		ServerPlayerDecalData_t() { V_memset( this, 0, sizeof(*this) ); }
		bool operator == ( ServerPlayerDecalData_t const &x ) const { return !V_memcmp( this, &x, offsetof( ServerPlayerDecalData_t, m_rtGcTime ) ); }

		AccountID_t m_unAccountID;
		int m_nTraceID;

		Vector m_vecOrigin;
		Vector m_vecStart;
		Vector m_vecRight;
		Vector m_vecNormal;
		int m_nEquipSlot;
		int m_nPlayer;
		int m_nEntity;
		int m_nHitbox;
		int m_nTintID;
		float m_flCreationTime;
		uint32 m_rtGcTime;	// not participating in memcmp (set by GC, authoritative)

		void CopyToMsg( CCSUsrMsg_PlayerDecalDigitalSignature &msg ) const;
		void InitFromMsg( CCSUsrMsg_PlayerDecalDigitalSignature const &msg );
	};
	CUtlVector< ServerPlayerDecalData_t > m_arrServerPlayerDecalData;
#endif

	int m_iAccountTerrorist;
	int m_iAccountCT;

	int m_iNumConsecutiveCTLoses;		//SupraFiend: the number of rounds the CTs have lost in a row.
	int m_iNumConsecutiveTerroristLoses;//SupraFiend: the number of rounds the Terrorists have lost in a row.

	int m_iSpawnPointCount_Terrorist;		// Number of Terrorist spawn points
	int m_iSpawnPointCount_CT;				// Number of CT spawn points

	int m_iMaxNumTerrorists;
	int m_iMaxNumCTs;

	int m_iLoserBonus;			// SupraFiend: the amount of money the losing team gets. This scales up as they lose more rounds in a row
	float m_tmNextPeriodicThink;
	
	bool m_bVoiceWonMatchBragFired;

	float m_fWarmupNextChatNoticeTime;

	CHandle<CCSPlayer> m_pMVP;
	
	// HOSTAGE RESCUE VARIABLES
	int		m_iHostagesRescued;
	int		m_iHostagesTouched;
	float	m_flNextHostageAnnouncement;

	// [tj] Accessor for weapons donation ability
	bool GetCanDonateWeapon( void ) { return m_bCanDonateWeapons; }

	// [tj] flawless and lossless round related flags
	bool m_bNoTerroristsKilled;
	bool m_bNoCTsKilled;
	bool m_bNoTerroristsDamaged;
	bool m_bNoCTsDamaged;
	bool m_bNoEnemiesKilled;

	// [tj] Find out if dropped weapons count as donations
	bool m_bCanDonateWeapons;

	// [tj] Keep track of first kill
	CHandle<CCSPlayer> m_pFirstKill;
	float m_firstKillTime;

	// [menglish] Keep track of first blood
	CHandle<CCSPlayer> m_pFirstBlood;
	float m_firstBloodTime;

	// [dwenger] Rescue-related achievement values
	CUtlVector< CHandle<CCSPlayer> > m_arrRescuers;

	bool m_hostageWasInjured;
	bool m_hostageWasKilled;

	// [menglish] Fun Fact Manager
	CCSFunFactMgr *m_pFunFactManager;

	// Automatic vote called near the end of a map
	bool	m_bVoteCalled;
	bool	m_bServerVoteOnReset;
	float	m_flVoteCheckThrottle;

	// [tj] To avoid rewriting the same piece of code, we can get all the information
	//		we want from one call that fills in an array of structures.
	struct TeamPlayerCounts
	{
		int totalPlayers;
		int totalAlivePlayers;
		int totalDeadPlayers; //sum of killedPlayers + suicidedPlayers + unenteredPlayers
		int killedPlayers;
		int suicidedPlayers;
		int unenteredPlayers;
	};

	void GetPlayerCounts( TeamPlayerCounts teamCounts[TEAM_MAXCOUNT] );

	bool m_bBuyTimeEnded;

	int m_nLastFreezeEndBeep;

	// PRISON ESCAPE VARIABLES
	int		m_iHaveEscaped;
	bool	m_bMapHasEscapeZone;
	int		m_iNumEscapers;			
	int		m_iNumEscapeRounds;		// keeps track of the # of consecutive rounds of escape played.. Teams will be swapped after 8 rounds

	
	// BOMB MAP VARIABLES
	bool	m_bTargetBombed;	// whether or not the bomb has been bombed
	bool	m_bBombDefused;	// whether or not the bomb has been defused
	bool	m_bMapHasBombZone;
/*	bool	m_bBombPlanted;*/

	bool	m_bGunGameRespawnWithBomb;	// Whether or not the next terrorist to spawn should have the bomb
	float	m_fGunGameBombRespawnTimer;	// Time until the bomb can be respawned

	void			AddTeamAccount( int team, TeamCashAward::Type reason );
	void			AddTeamAccount( int team, TeamCashAward::Type reason, int amount, const char* szAwardText = NULL );

public:
	CBaseEntity* GetNextSpawnpoint( int teamNumber );

	void DoCoopSpawnAndNavInit( void );
	void AddSpawnPointToMasterList( SpawnPoint* pSpawnPoint );
	void GenerateSpawnPointListsFirstTime( void );
	void RefreshCurrentSpawnPointLists( void );

	CCSMatch		m_match;

	Vector	m_vecMainCTSpawnPos;

	void ShuffleSpawnPointLists( void );
	void ShuffleMasterSpawnPointLists( void );
	void SortSpawnPointLists( void );
	void SortMasterSpawnPointLists( void );
	void ShufflePlayerList( CUtlVector< CCSPlayer* > &playersList );
	
protected:
	CUtlVector< SpawnPoint* >	m_CTSpawnPointsMasterList;			// The master list of CT spawn points (contains all points whether enabled or disabled)
	CUtlVector< SpawnPoint* >	m_TerroristSpawnPointsMasterList;	// The master list of Terrorist spawn points (contains all points whether enabled or disabled)

	int m_iNextCTSpawnPoint;						// Used when picking the next CT spawn point to assign
	int m_iNextTerroristSpawnPoint;					// Used when picking the next Terrorist spawn point to assign
	CUtlVector< SpawnPoint* >	m_CTSpawnPoints;		// List of CT spawn points sorted by their priorities
	CUtlVector< SpawnPoint* >	m_TerroristSpawnPoints;	// List of Terrorist spawn points sorted by their priorities

private:
	
	float m_fAutobalanceDisplayTime;
	AutobalanceStatus::Type m_AutobalanceStatus;
	CRecipientFilter m_AutoBalanceTraitors;
	CRecipientFilter m_AutoBalanceLoyalists;

	// Don't allow switching weapons while gaining new technologies
	bool			m_bAllowWeaponSwitch;

	bool			m_bRoundTimeWarningTriggered;
	
	float			m_phaseChangeAnnouncementTime;
	float			m_fNextUpdateTeamClanNamesTime;

	float			m_flLastThinkTime;

	CRecipientFilter m_filterTerrorist;
	CRecipientFilter m_filterCT;


#endif

	// Methods & memeber variables on both client and server DLLs

public:

	virtual bool ForceSplitScreenPlayersOnToSameTeam( void );

	void UpdatePlayerEloBracket( CCSPlayer *pPlayer, int32 bracket );

	bool IsSwitchingTeamsAtRoundReset( void ) { return m_bSwitchingTeamsAtRoundReset; }

	float CheckTotalSmokedLength( float flRadius, Vector vecGrenadePos, Vector from, Vector to );

protected:
	loadout_positions_t PickRandomWeaponForDMBonus( void );

	void AssignStartingMoneyToAllPlayers( void );

	void InitializeGameTypeAndMode( void );

	bool m_bHasTriggeredRoundStartMusic;
	bool m_bHasTriggeredCoopSpawnReset;

public:
	BotProfileDevice_t GetMatchDevice( void ) const { return ( BotProfileDevice_t )( m_MatchDevice.Get() ); }

	void AddDroppedWeaponToList( CWeaponCSBase *pWeapon );
	void RemoveDroppedWeaponFromList( CWeaponCSBase *pWeapon );
	int GetTotalDroppedWeaponsInWorld( void ) { return m_weaponsDroppedInWorld.Count(); }

protected:
	void ProcessAutoBalance( void );

private:
	void UnfreezeAllPlayers( void );

	bool m_bSwitchingTeamsAtRoundReset;
	int m_iMaxGunGameProgressiveWeaponIndex;

	CUtlVector< CHandle<CWeaponCSBase> > m_weaponsDroppedInWorld;
};

bool EconEntity_OnOwnerKillEaterEvent( CEconItemView *pEconItemView, CCSPlayer *pOwner, CCSPlayer *pVictim, kill_eater_event_t eEventType, int iAmount = 1, uint32 *pNewValue = NULL );

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CCSGameRules* CSGameRules()
{
	return static_cast<CCSGameRules*>(g_pGameRules);
}

#define IGNORE_SPECTATORS false
#define IGNORE_UNASSIGNED true
int UTIL_HumansInGame( bool ignoreSpectators = false, bool ignoreUnassigned = false );
int UTIL_SpectatorsInGame( void );

//-----------------------------------------------------------------------------
// Music Selection
//-----------------------------------------------------------------------------

enum CsMusicType_t
{
	CSMUSIC_NONE = 0,
	CSMUSIC_STARTGG,
	CSMUSIC_START,
	CSMUSIC_ACTION,
	CSMUSIC_DEATHCAM,
	CSMUSIC_BOMB,
	CSMUSIC_BOMBTEN,
	CSMUSIC_ROUNDTEN,
	CSMUSIC_WONROUND,
	CSMUSIC_LOSTROUND,
	CSMUSIC_HOSTAGE,
	CSMUSIC_MVP,
	CSMUSIC_SELECTION,
	CSMUSIC_HALFTIME

};
#define CSMUSIC_NOPACK 0

#ifdef CLIENT_DLL
void PlayMusicSelection( IRecipientFilter& filter, CsMusicType_t nMusicType , int nPlayerEntIndex  = 0 , float flPreElapsedTime  = 0.0 );
#endif

//-----------------------------------------------------------------------------
// Purpose: Useful utility functions
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL

#else
	
	class CTFTeam;
	CTFTeam *GetOpposingTeam( CTeam *pTeam );
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround, unsigned int mask = MASK_SOLID, ITraceFilter *pFilter = NULL );

#endif

// Assassination quest helper funcs
bool IsAssassinationQuest( const CEconQuestDefinition *pQuest );
bool IsAssassinationQuest( uint32 questID );
bool IsAssassinationQuestActive( const CEconQuestDefinition *pQuest );

extern const float g_flWarmupToFreezetimeDelay;

#endif // TF_GAMERULES_H

