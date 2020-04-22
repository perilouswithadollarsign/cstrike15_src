//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#if !defined ( ACHIEVEMENTS_CS_H )
#define ACHIEVEMENTS_CS_H

#include "cbase.h"
#include "cs_gamestats_shared.h"
#include "baseachievement.h"

#ifdef _PS3
#include "steam/steam_api.h"
#endif

#ifdef CLIENT_DLL

bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole );
bool IsLocalCSPlayerClass( int iClass );
bool GameRulesAllowsAchievements( void );

//----------------------------------------------------------------------------------------------------------------
// Base class for all CS achievements
class CCSBaseAchievement : public CBaseAchievement
{
	DECLARE_CLASS( CCSBaseAchievement, CBaseAchievement );
public:

	CCSBaseAchievement();

	virtual void GetSettings( KeyValues* pNodeOut );				// serialize
	virtual void ApplySettings( /* const */ KeyValues* pNodeIn );	// unserialize

	// [dwenger] Necessary for sorting achievements by award time
	virtual void OnAchieved();
	bool GetAwardTime( int& year, int& month, int& day, int& hour, int& minute, int& second );

	int64 GetSortKey() const { return GetUnlockTime(); }
};


//----------------------------------------------------------------------------------------------------------------
// Helper class for achievements that check that the player was playing on a game team for the full round
class CCSBaseAchievementFullRound : public CCSBaseAchievement
{
	DECLARE_CLASS( CCSBaseAchievementFullRound, CCSBaseAchievement );
public:
	virtual void Init() ;
	virtual void ListenForEvents();
	void FireGameEvent_Internal( IGameEvent *event );
	bool PlayerWasInEntireRound( float flRoundTime );

	virtual void Event_OnRoundComplete( float flRoundTime, IGameEvent *event ) = 0 ;
};


//----------------------------------------------------------------------------------------------------------------
// Helper class for achievements based on other achievements
class CAchievement_Meta : public CCSBaseAchievement
{
	DECLARE_CLASS( CAchievement_Meta, CCSBaseAchievement );
public:
	CAchievement_Meta();
	virtual void Init();

#if !defined(NO_STEAM)
	STEAM_CALLBACK( CAchievement_Meta, Steam_OnUserAchievementStored, UserAchievementStored_t, m_CallbackUserAchievement );
#endif

protected:
	void AddRequirement( int nAchievementId );

private:
	CUtlVector<int> m_requirements;
};

//Base class for all achievements to kill x players with a given weapon
class CAchievement_StatGoal: public CCSBaseAchievement
{
public:
	void SetStatId(CSStatType_t stat)
	{
		m_StatId = stat;
	}
	CSStatType_t GetStatId(void)
	{
		return m_StatId;
	}
private:
	virtual void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );		
	}	

	void OnPlayerStatsUpdate( int nUserSlot );
	CSStatType_t m_StatId;
};

#if defined (_X360)
static const int NumXboxMappedAchievements = 11; // number of xbox achievements that have a one to one correspondence with a medal
static const int MedalToXBox[NumXboxMappedAchievements][2] = { { CSEnemyKillsHigh, ACHIEVEMENT_KILL_ENEMY_HIGH }, 
{ CSGiveDamageLow, ACHIEVEMENT_GIVE_DAMAGE_LOW }, 
{ CSMoneyEarnedLow, ACHIEVEMENT_EARN_MONEY_LOW }, 
{ CSGunGameProgressiveRampage, ACHIEVEMENT_GUN_GAME_RAMPAGE }, 
{ CSHeadshots, ACHIEVEMENT_HEADSHOTS }, 
{ CSFlawlessVictory, ACHIEVEMENT_FLAWLESS_VICTORY }, 
{ CSDominationsLow, ACHIEVEMENT_DOMINATIONS_LOW }, 
{ CSKillWithEveryWeapon, ACHIEVEMENT_KILL_WITH_EVERY_WEAPON }, 
{ CSKillEnemyLastBullet, ACHIEVEMENT_KILL_ENEMY_LAST_BULLET }, 
{ CSKillEnemyTerrTeamBeforeBombPlant, ACHIEVEMENT_GUN_GAME_FIRST_THING_FIRST }, 
{ CSGGRoundsHigh, ACHIEVEMENT_GUN_GAME_ROUNDS_HIGH }};

class CAchievementListener : public CAutoGameSystem, public CGameEventListener
{
public:
	CAchievementListener();

	virtual bool Init();

protected:
	void FireGameEvent( IGameEvent *event );

};
#endif


extern CAchievementMgr g_AchievementMgrCS;	// global achievement mgr for CS

#endif // CLIENT_DLL

#endif // ACHIEVEMENTS_CS_H