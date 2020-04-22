//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include <time.h>

#include "achievementmgr.h"

#ifdef GAME_DLL
CAchievementMgr * CAchievementMgr::GetInstance()
{
	IAchievementMgr *pMgr = engine->GetAchievementMgr();
	class CAchievementMgrDelegateIAchievementMgr_friend : public CAchievementMgrDelegateIAchievementMgr
	{
	public: CAchievementMgr * GetDelegate() const { return m_pDelegate; }
	private: CAchievementMgrDelegateIAchievementMgr_friend() : CAchievementMgrDelegateIAchievementMgr( NULL ) {}
	};
	return reinterpret_cast< CAchievementMgrDelegateIAchievementMgr_friend * >( pMgr )->GetDelegate();
}
IAchievementMgr * CAchievementMgr::GetInstanceInterface()
{
	return engine->GetAchievementMgr();
}
#endif // GAME_DLL

#ifdef CLIENT_DLL

#include "baseachievement.h"
#include "cs_achievement_constants.h"
#include "c_cs_team.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "cs_gamerules.h"
#include "achievements_cs.h"
#include "cs_client_gamestats.h"

// [dwenger] Necessary for sorting achievements by award time
#include <vgui/ISystem.h>
#include "../../src/public/vgui_controls/Controls.h"

#endif // CLIENT_DLL

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL

ConVar achievements_easymode( "achievement_easymode", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY,
						 "Enables all stat-based achievements to be earned at 10% of goals" );

// global achievement mgr for CS
CAchievementMgr g_AchievementMgrCS;		

CAchievementMgrDelegateIAchievementMgr g_IAchievementMgrImpl( &g_AchievementMgrCS );
CAchievementMgr * CAchievementMgr::GetInstance()
{
#ifdef _DEBUG
	IAchievementMgr *pMgr = engine->GetAchievementMgr();
	Assert( !pMgr || ( pMgr == &g_IAchievementMgrImpl ) );
#endif
	return &g_AchievementMgrCS;
}
IAchievementMgr * CAchievementMgr::GetInstanceInterface()
{
#ifdef _DEBUG
	IAchievementMgr *pMgr = engine->GetAchievementMgr();
	Assert( !pMgr || ( pMgr == &g_IAchievementMgrImpl ) );
	Assert( ((uintp)static_cast<IAchievementMgr*>(&g_IAchievementMgrImpl)) == ((uintp)(&g_IAchievementMgrImpl)) );
#endif
	return &g_IAchievementMgrImpl;
}


CCSBaseAchievement::CCSBaseAchievement()
{
}

//-----------------------------------------------------------------------------
// Purpose: Determines the timestamp when the achievement is awarded
//-----------------------------------------------------------------------------
void CCSBaseAchievement::OnAchieved()
{
#if defined( _X360 )
 	__time32_t unlockTime;
 	_time32(&unlockTime);
 	SetUnlockTime(unlockTime);
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns the time values when the achievement was awarded.
//-----------------------------------------------------------------------------
bool CCSBaseAchievement::GetAwardTime( int& year, int& month, int& day, int& hour, int& minute, int& second )
{
	if ( GetUnlockTime() )
	{
#if defined( _PS3 ) || defined( _OSX ) || defined (LINUX)
		time_t timeOfDay = (time_t) GetUnlockTime( );

		// [dkorus] note, structuredTime is a pointer to a static structure that will last until another time structure function is called.
		struct tm* structuredTimePtr = localtime( &timeOfDay );
#else
		struct tm structuredTime;
		__time32_t unlockTime = static_cast<__time32_t>(GetUnlockTime());
		_localtime32_s(&structuredTime, &unlockTime);
		struct tm* structuredTimePtr = &structuredTime;
#endif
		if ( structuredTimePtr )
		{
			year = structuredTimePtr->tm_year + 1900;
			month = structuredTimePtr->tm_mon + 1;	// 0..11
			day = structuredTimePtr->tm_mday;
			hour = structuredTimePtr->tm_hour;
			minute = structuredTimePtr->tm_min;
			second = structuredTimePtr->tm_sec;
		}

		return true;
	}
	else
	{
		return false;
	}
}

void CCSBaseAchievement::GetSettings( KeyValues* pNodeOut )
{
	BaseClass::GetSettings(pNodeOut);

	pNodeOut->SetInt("unlockTime", GetUnlockTime());
}

void CCSBaseAchievement::ApplySettings( /* const */ KeyValues* pNodeIn )
{
	BaseClass::ApplySettings(pNodeIn);

	SetUnlockTime(pNodeIn->GetInt("unlockTime"));
}


/**
* Meta Achievement base class methods
*/
#if (!defined NO_STEAM)
CAchievement_Meta::CAchievement_Meta() :
	m_CallbackUserAchievement( this, &CAchievement_Meta::Steam_OnUserAchievementStored )
{
}
#else
CAchievement_Meta::CAchievement_Meta()
{
}
#endif

void CAchievement_Meta::Init()
{
	SetFlags( ACH_SAVE_GLOBAL );
	SetGoal( 1 );
}

// $FIXME(hpe) how do we award achievements in the no steam case on X360
#if (!defined NO_STEAM)
void CAchievement_Meta::Steam_OnUserAchievementStored( UserAchievementStored_t *pUserAchievementStored )
{
	if ( IsAchieved() )
		return;

	int iAchieved = 0;

	FOR_EACH_VEC(m_requirements, i)
	{
		IAchievement* pAchievement = (IAchievement*)m_pAchievementMgr->GetAchievementByID(m_requirements[i], m_nUserSlot);
		Assert ( pAchievement );

		if ( pAchievement->IsAchieved() )
			iAchieved++;
		else
			break;
	}

	if ( iAchieved == m_requirements.Count() )
	{
		AwardAchievement();
	}
}
#endif


void CAchievement_StatGoal::OnPlayerStatsUpdate( int nUserSlot )
{
	// when stats are updated by server, use most recent stat value
	const StatsCollection_t& roundStats = g_CSClientGameStats.GetLifetimeStats( nUserSlot );

	int iOldCount = GetCount();
	SetCount(roundStats[m_StatId]);
	if ( GetCount() != iOldCount )
	{
		m_pAchievementMgr->SetDirty(true, m_nUserSlot);
	}

	int iGoal = GetGoal();
	if (!IsAchieved() && iGoal > 0 )
	{
		if (achievements_easymode.GetBool())
		{
			iGoal /= 10;
			if ( iGoal == 0 )
				iGoal = 1;
		}

		if ( GetCount() >= iGoal )
		{
			AwardAchievement();
		}
	}
}

void CAchievement_Meta::AddRequirement( int nAchievementId )
{
	m_requirements.AddToTail(nAchievementId);
}

#if 0
bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole );

// Grace period that we allow a player to start after level init and still consider them to be participating for the full round.  This is fairly generous
// because it can in some cases take a client several minutes to connect with respect to when the server considers the game underway
#define CS_FULL_ROUND_GRACE_PERIOD	( 4 * 60.0f )

bool IsLocalCSPlayerClass( int iClass );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseAchievementFullRound::Init() 
{
	m_iFlags |= ACH_FILTER_FULL_ROUND_ONLY;		
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseAchievementFullRound::ListenForEvents()
{
	ListenForGameEvent( "teamplay_round_win" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseAchievementFullRound::FireGameEvent_Internal( IGameEvent *event )
{
	if ( 0 == Q_strcmp( event->GetName(), "teamplay_round_win" ) )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			// is the player currently on a game team?
			int iTeam = pLocalPlayer->GetTeamNumber();
			if ( iTeam >= FIRST_GAME_TEAM ) 
			{
				float flRoundTime = event->GetFloat( "round_time", 0 );
				if ( flRoundTime > 0 )
				{
					Event_OnRoundComplete( flRoundTime, event );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSBaseAchievementFullRound::PlayerWasInEntireRound( float flRoundTime )
{
	float flTeamplayStartTime = m_pAchievementMgr->GetTeamplayStartTime();
	if ( flTeamplayStartTime > 0 ) 
	{	
		// has the player been present and on a game team since the start of this round (minus a grace period)?
		if ( flTeamplayStartTime < ( gpGlobals->curtime - flRoundTime ) + CS_FULL_ROUND_GRACE_PERIOD )
			return true;
	}
	return false;
}
#endif

#define DECLARE_ACHIEVEMENT_STATGOAL( achievementID, achievementName, iPointValue, iStatId, iGoal ) \
	static CBaseAchievement *Create_##achievementID( void )			\
{																		\
	CAchievement_StatGoal *pAchievement = new CAchievement_StatGoal();	\
	pAchievement->SetAchievementID( achievementID );					\
	pAchievement->SetName( achievementName );							\
	pAchievement->SetPointValue( iPointValue );							\
	pAchievement->SetHideUntilAchieved( false );						\
	pAchievement->SetStatId(iStatId);									\
	pAchievement->SetGoal( iGoal );										\
	return pAchievement;												\
};																		\
static CBaseAchievementHelper g_##achievementID##_Helper( Create_##achievementID );			

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsLow,		    "KILL_ENEMY_LOW",			10,	CSSTAT_KILLS,					25); //25
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMed,		    "KILL_ENEMY_MED",			10,	CSSTAT_KILLS,					500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsHigh,		    "KILL_ENEMY_HIGH",			10,	CSSTAT_KILLS,					10000); //10000

DECLARE_ACHIEVEMENT_STATGOAL(CSWinRoundsLow,			"WIN_ROUNDS_LOW",			10,	CSSTAT_ROUNDS_WON,				10); //10
DECLARE_ACHIEVEMENT_STATGOAL(CSWinRoundsMed,            "WIN_ROUNDS_MED",			10,	CSSTAT_ROUNDS_WON,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSWinRoundsHigh,		    "WIN_ROUNDS_HIGH",			10,	CSSTAT_ROUNDS_WON,				5000); //5000

DECLARE_ACHIEVEMENT_STATGOAL(CSGGWinRoundsLow,			"WIN_GUN_GAME_ROUNDS_LOW",	10,	CSSTAT_GUN_GAME_MATCHES_WON,				1); 
DECLARE_ACHIEVEMENT_STATGOAL(CSGGWinRoundsMed,          "WIN_GUN_GAME_ROUNDS_MED",			10,	CSSTAT_GUN_GAME_MATCHES_WON,				25); 
DECLARE_ACHIEVEMENT_STATGOAL(CSGGWinRoundsHigh,		    "WIN_GUN_GAME_ROUNDS_HIGH",		10,	CSSTAT_GUN_GAME_MATCHES_WON,				100); 
DECLARE_ACHIEVEMENT_STATGOAL(CSGGWinRoundsExtreme,	    "WIN_GUN_GAME_ROUNDS_EXTREME",		10,	CSSTAT_GUN_GAME_MATCHES_WON,				500); 
DECLARE_ACHIEVEMENT_STATGOAL(CSGGWinRoundsUltimate,	    "WIN_GUN_GAME_ROUNDS_ULTIMATE",	10,	CSSTAT_GUN_GAME_MATCHES_WON,			1000); 

DECLARE_ACHIEVEMENT_STATGOAL(CSGGRoundsLow,				"GUN_GAME_ROUNDS_LOW",		10,	CSSTAT_GUN_GAME_MATCHES_PLAYED,			100); 
DECLARE_ACHIEVEMENT_STATGOAL(CSGGRoundsMed,				"GUN_GAME_ROUNDS_MED",		10,	CSSTAT_GUN_GAME_MATCHES_PLAYED,			500); 
DECLARE_ACHIEVEMENT_STATGOAL(CSGGRoundsHigh,			"GUN_GAME_ROUNDS_HIGH",		10,	CSSTAT_GUN_GAME_MATCHES_PLAYED,			5000); 
	
DECLARE_ACHIEVEMENT_STATGOAL(CSWinPistolRoundsLow,	    "WIN_PISTOLROUNDS_LOW",		10,	CSSTAT_PISTOLROUNDS_WON,		5); //5
DECLARE_ACHIEVEMENT_STATGOAL(CSWinPistolRoundsMed,	    "WIN_PISTOLROUNDS_MED",		10,	CSSTAT_PISTOLROUNDS_WON,		25); //25
DECLARE_ACHIEVEMENT_STATGOAL(CSWinPistolRoundsHigh,	    "WIN_PISTOLROUNDS_HIGH",	10,	CSSTAT_PISTOLROUNDS_WON,		250); //250

DECLARE_ACHIEVEMENT_STATGOAL(CSMoneyEarnedLow,		    "EARN_MONEY_LOW",			10,	CSSTAT_MONEY_EARNED,			50000);    //125000
DECLARE_ACHIEVEMENT_STATGOAL(CSMoneyEarnedMed,		    "EARN_MONEY_MED",			10,	CSSTAT_MONEY_EARNED,			2500000);   //2500000
DECLARE_ACHIEVEMENT_STATGOAL(CSMoneyEarnedHigh,		    "EARN_MONEY_HIGH",			10,	CSSTAT_MONEY_EARNED,			50000000);  //50000000

DECLARE_ACHIEVEMENT_STATGOAL(CSGiveDamageLow,		    "GIVE_DAMAGE_LOW",			10,	CSSTAT_DAMAGE,					2500); //2500
DECLARE_ACHIEVEMENT_STATGOAL(CSGiveDamageMed,		    "GIVE_DAMAGE_MED",			10,	CSSTAT_DAMAGE,					50000); //50000
DECLARE_ACHIEVEMENT_STATGOAL(CSGiveDamageHigh,		    "GIVE_DAMAGE_HIGH",			10,	CSSTAT_DAMAGE,					1000000); //1000000

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsDeagle,		"KILL_ENEMY_DEAGLE",		5,	CSSTAT_KILLS_DEAGLE,			200); //200
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsUSP,		    "KILL_ENEMY_USP",			5,	CSSTAT_KILLS_USP,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsGlock,		    "KILL_ENEMY_GLOCK",			5,	CSSTAT_KILLS_GLOCK,				100); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsHKP2000,	    "KILL_ENEMY_HKP2000",		5,	CSSTAT_KILLS_HKP2000,			100); 
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsP228,		    "KILL_ENEMY_P228",			5,	CSSTAT_KILLS_P228,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsElite,		    "KILL_ENEMY_ELITE",			5,	CSSTAT_KILLS_ELITE,				25); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsFiveSeven,	    "KILL_ENEMY_FIVESEVEN",		5,	CSSTAT_KILLS_FIVESEVEN,			25); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsP250,		    "KILL_ENEMY_P250",			5,	CSSTAT_KILLS_P250,				25); 

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsTaser,			"KILL_ENEMY_TASER",			5,	CSSTAT_KILLS_TASER,				10); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsAWP,		    "KILL_ENEMY_AWP",			5,	CSSTAT_KILLS_AWP,				500); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsAK47,		    "KILL_ENEMY_AK47",			5,	CSSTAT_KILLS_AK47,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsM4A1,		    "KILL_ENEMY_M4A1",			5,	CSSTAT_KILLS_M4A1,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsAUG,		    "KILL_ENEMY_AUG",			5,	CSSTAT_KILLS_AUG,				250); //500
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSG552,		    "KILL_ENEMY_SG552",         5,	CSSTAT_KILLS_SG552,				500); //500
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSG550,		    "KILL_ENEMY_SG550",			5,	CSSTAT_KILLS_SG550,				500); //500
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsGALIL,		    "KILL_ENEMY_GALIL",			5,	CSSTAT_KILLS_GALIL,				500); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsGALILAR,		"KILL_ENEMY_GALILAR",		5,	CSSTAT_KILLS_GALILAR,			250); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsFAMAS,		    "KILL_ENEMY_FAMAS",			5,	CSSTAT_KILLS_FAMAS,				100); //500
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsScout,		    "KILL_ENEMY_SCOUT",			5,	CSSTAT_KILLS_SCOUT,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsG3SG1,		    "KILL_ENEMY_G3SG1",			5,	CSSTAT_KILLS_G3SG1,				100); //500
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSCAR17,	    "KILL_ENEMY_SCAR17",		5,	CSSTAT_KILLS_SCAR17,			1000); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSCAR20,	    "KILL_ENEMY_SCAR20",		5,	CSSTAT_KILLS_SCAR20,			100); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSG556,		    "KILL_ENEMY_SG556",			5,	CSSTAT_KILLS_SG556,				100); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSSG08,		    "KILL_ENEMY_SSG08",			5,	CSSTAT_KILLS_SSG08,				100); 

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsP90,		    "KILL_ENEMY_P90",			5,	CSSTAT_KILLS_P90,				500); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsTec9,			"KILL_ENEMY_TEC9",			5,	CSSTAT_KILLS_TEC9,				100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsBizon,			"KILL_ENEMY_BIZON",			5,	CSSTAT_KILLS_BIZON,				250); 
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMP5NAVY,	    "KILL_ENEMY_MP5NAVY",		5,	CSSTAT_KILLS_MP5NAVY,			1000); //1000
//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsTMP,		    "KILL_ENEMY_TMP",			5,	CSSTAT_KILLS_TMP,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMAC10,		    "KILL_ENEMY_MAC10",			5,	CSSTAT_KILLS_MAC10,				100); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsUMP45,		    "KILL_ENEMY_UMP45",			5,	CSSTAT_KILLS_UMP45,				250); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMP7,		    "KILL_ENEMY_MP7",			5,	CSSTAT_KILLS_MP7,				250);
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMP9,		    "KILL_ENEMY_MP9",			5,	CSSTAT_KILLS_MP9,				100); 

//DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsM3,			"KILL_ENEMY_M3",			5,	CSSTAT_KILLS_M3,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsXM1014,		"KILL_ENEMY_XM1014",		5,	CSSTAT_KILLS_XM1014,			200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMag7,			"KILL_ENEMY_MAG7",			5,	CSSTAT_KILLS_MAG7,				50); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSawedoff,		"KILL_ENEMY_SAWEDOFF",		5,	CSSTAT_KILLS_SAWEDOFF,			50);
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsNova,			"KILL_ENEMY_NOVA",			5,	CSSTAT_KILLS_NOVA,				100);

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsM249,		    "KILL_ENEMY_M249",			5,	CSSTAT_KILLS_M249,				100); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsNegev,		    "KILL_ENEMY_NEGEV",			5,	CSSTAT_KILLS_NEGEV,				100); 
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsKnife,		    "KILL_ENEMY_KNIFE",			5,	CSSTAT_KILLS_KNIFE,				100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsHEGrenade,	    "KILL_ENEMY_HEGRENADE",		5,	CSSTAT_KILLS_HEGRENADE,			100); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMolotov,	    "KILL_ENEMY_MOLOTOV",		5,	CSSTAT_KILLS_MOLOTOV,			100);

DECLARE_ACHIEVEMENT_STATGOAL(CSHeadshots,				"HEADSHOTS",				5,	CSSTAT_KILLS_HEADSHOT,			250); //250
DECLARE_ACHIEVEMENT_STATGOAL(CSKillsEnemyWeapon,		"KILLS_ENEMY_WEAPON",		5,	CSSTAT_KILLS_ENEMY_WEAPON,		100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSKillEnemyBlinded,		"KILL_ENEMY_BLINDED",		5,	CSSTAT_KILLS_ENEMY_BLINDED,		25); //25

DECLARE_ACHIEVEMENT_STATGOAL(CSDefuseBombsLow,          "BOMB_DEFUSE_LOW",			5,	CSSTAT_NUM_BOMBS_DEFUSED,		100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSPlantBombsLow,		    "BOMB_PLANT_LOW",			5,	CSSTAT_NUM_BOMBS_PLANTED,		100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSDefuseBombsTRLow,        "TR_BOMB_DEFUSE_LOW",		5,	CSSTAT_TR_NUM_BOMBS_DEFUSED,	5); 
DECLARE_ACHIEVEMENT_STATGOAL(CSPlantBombsTRLow,		    "TR_BOMB_PLANT_LOW",		5,	CSSTAT_TR_NUM_BOMBS_PLANTED,	5); 

DECLARE_ACHIEVEMENT_STATGOAL(CSRescueHostagesLow,	    "RESCUE_HOSTAGES_LOW",		5,	CSSTAT_NUM_HOSTAGES_RESCUED,	100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSRescueHostagesMid,	    "RESCUE_HOSTAGES_MED",		5,	CSSTAT_NUM_HOSTAGES_RESCUED,	500); //500

DECLARE_ACHIEVEMENT_STATGOAL(CSWinKnifeFightsLow,       "WIN_KNIFE_FIGHTS_LOW",		5,  CSSTAT_KILLS_KNIFE_FIGHT,       1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSWinKnifeFightsHigh,      "WIN_KNIFE_FIGHTS_HIGH",	5,  CSSTAT_KILLS_KNIFE_FIGHT,       100); //100

//DECLARE_ACHIEVEMENT_STATGOAL(CSDecalSprays,             "DECAL_SPRAYS",				5,	CSSTAT_DECAL_SPRAYS,            100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSKillSnipers,             "KILL_SNIPERS",				5,	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, 100); //100

//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_ASSAULT,        "WIN_MAP_CS_ASSAULT",       5,  CSSTAT_MAP_WINS_CS_ASSAULT,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_ITALY,          "WIN_MAP_CS_ITALY",         5,  CSSTAT_MAP_WINS_CS_ITALY,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_OFFICE,         "WIN_MAP_CS_OFFICE",        5,  CSSTAT_MAP_WINS_CS_OFFICE,      100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_AZTEC,          "WIN_MAP_DE_AZTEC",         5,  CSSTAT_MAP_WINS_DE_AZTEC,       100); //100
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_CBBLE,          "WIN_MAP_DE_CBBLE",         5,  CSSTAT_MAP_WINS_DE_CBBLE,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_DUST2,          "WIN_MAP_DE_DUST2",         5,  CSSTAT_MAP_WINS_DE_DUST2,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_DUST,           "WIN_MAP_DE_DUST",          5,  CSSTAT_MAP_WINS_DE_DUST,        100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_INFERNO,        "WIN_MAP_DE_INFERNO",       5,  CSSTAT_MAP_WINS_DE_INFERNO,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_NUKE,           "WIN_MAP_DE_NUKE",          5,  CSSTAT_MAP_WINS_DE_NUKE,        100); //100
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_PIRANESI,       "WIN_MAP_DE_PIRANESI",      5,  CSSTAT_MAP_WINS_DE_PIRANESI,    100); //100
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_PRODIGY,        "WIN_MAP_DE_PRODIGY",       5,  CSSTAT_MAP_WINS_DE_PRODIGY,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_TRAIN,			"WIN_MAP_DE_TRAIN",			5,  CSSTAT_MAP_WINS_DE_TRAIN,		100); //100

// anticipating medals for these.  Just uncomment and add strings to cstrike15_english to enable
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_VERTIGO,        "WIN_MAP_DE_VERTIGO",       5,  CSSTAT_MAP_WINS_DE_VERTIGO,       100); //100
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_BALKAN,         "WIN_MAP_DE_BALKAN",        5,  CSSTAT_MAP_WINS_DE_BALKAN,       100); //100
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapAR_MONASTERY,	  "WIN_MAP_AR_MONASTERY",     5,  CSSTAT_MAP_WINS_AR_MONASTERY,    5); //100

// gungame maps
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_EMBASSY,       "WIN_MAP_DE_EMBASSY",      5,  CSSTAT_MAP_MATCHES_WON_EMBASSY,    5); //100
//DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_DEPOT,          "WIN_MAP_DE_DEPOT",         5,  CSSTAT_MAP_MATCHES_WON_DEPOT,       5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_BANK,			"WIN_MAP_DE_BANK",			5,  CSSTAT_MAP_MATCHES_WON_BANK,        5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchAR_SHOOTS,		"WIN_MAP_AR_SHOOTS",		5,  CSSTAT_MAP_MATCHES_WON_SHOOTS,        5); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_STMARC,         "WIN_MAP_DE_STMARC",        5,  CSSTAT_MAP_MATCHES_WON_STMARC,      5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_SUGARCANE,	"WIN_MAP_DE_SUGARCANE",		5,  CSSTAT_MAP_MATCHES_WON_SUGARCANE,     5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_SAFEHOUSE,    "WIN_MAP_DE_SAFEHOUSE",     5,  CSSTAT_MAP_MATCHES_WON_SAFEHOUSE,       5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_LAKE,			"WIN_MAP_DE_LAKE",			5,  CSSTAT_MAP_MATCHES_WON_LAKE,			5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchAR_BAGGAGE,		"WIN_MAP_AR_BAGGAGE",		5,  CSSTAT_MAP_MATCHES_WON_BAGGAGE,        5); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMatchDE_SHORTTRAIN,	"WIN_MAP_DE_SHORTTRAIN",	5,  CSSTAT_MAP_MATCHES_WON_SHORTTRAIN,     5); //100


DECLARE_ACHIEVEMENT_STATGOAL(CSDonateWeapons,           "DONATE_WEAPONS",           5,  CSSTAT_WEAPONS_DONATED,         100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSDominationsLow,          "DOMINATIONS_LOW",          5,  CSSTAT_DOMINATIONS,             1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSDominationsHigh,         "DOMINATIONS_HIGH",         5,  CSSTAT_DOMINATIONS,             10); //10
DECLARE_ACHIEVEMENT_STATGOAL(CSDominationOverkillsLow,  "DOMINATION_OVERKILLS_LOW", 5,  CSSTAT_DOMINATION_OVERKILLS,    1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSDominationOverkillsHigh, "DOMINATION_OVERKILLS_HIGH",5,  CSSTAT_DOMINATION_OVERKILLS,    100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSRevengesLow,             "REVENGES_LOW",             5,  CSSTAT_REVENGES,                1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSRevengesHigh,            "REVENGES_HIGH",            5,  CSSTAT_REVENGES,                20); //20

//-----------------------------------------------------------------------------
// Purpose: Generic server awarded achievement
//-----------------------------------------------------------------------------
class CAchievementCS_ServerAwarded : public CCSBaseAchievement
{
	virtual void Init() 
	{
		SetGoal(1);
		SetFlags( ACH_SAVE_GLOBAL );
	}

	// server fires an event for this achievement, no other code within achievement necessary
};
#define DECLARE_ACHIEVEMENT_SERVERAWARDED(achievementID, achievementName, iPointValue) \
	static CBaseAchievement *Create_##achievementID( void )			\
{																		\
	CAchievementCS_ServerAwarded *pAchievement = new CAchievementCS_ServerAwarded( );					\
	pAchievement->SetAchievementID( achievementID );					\
	pAchievement->SetName( achievementName );							\
	pAchievement->SetPointValue( iPointValue );							\
	return pAchievement;												\
};																		\
static CBaseAchievementHelper g_##achievementID##_Helper( Create_##achievementID );



// server triggered achievements
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBombDefuseCloseCall, "BOMB_DEFUSE_CLOSE_CALL", 5);
//DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDefuseAndNeededKit, "BOMB_DEFUSE_NEEDED_KIT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKilledDefuser, "KILL_BOMB_DEFUSER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinBombPlant, "WIN_BOMB_PLANT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinBombDefuse, "WIN_BOMB_DEFUSE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSPlantBombWithin25Seconds, "BOMB_PLANT_IN_25_SECONDS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSRescueAllHostagesInARound, "RESCUE_ALL_HOSTAGES", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyWithFormerGun, "KILL_WITH_OWN_GUN", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillingSpree, "KILLING_SPREE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillTwoWithOneShot, "KILL_TWO_WITH_ONE_SHOT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyReloading, "KILL_ENEMY_RELOADING", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillsWithMultipleGuns, "KILLS_WITH_MULTIPLE_GUNS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSPosthumousGrenadeKill, "DEAD_GRENADE_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyTeam, "KILL_ENEMY_TEAM", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSLastPlayerAlive, "LAST_PLAYER_ALIVE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyLastBullet, "KILL_ENEMY_LAST_BULLET", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillingSpreeEnder, "KILLING_SPREE_ENDER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemiesWhileBlind, "KILL_ENEMIES_WHILE_BLIND", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemiesWhileBlindHard, "KILL_ENEMIES_WHILE_BLIND_HARD", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDamageNoKill, "DAMAGE_NO_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillLowDamage, "KILL_LOW_DAMAGE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKilledRescuer, "KILL_HOSTAGE_RESCUER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSurviveGrenade, "SURVIVE_GRENADE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKilledDefuserWithGrenade, "KILLED_DEFUSER_WITH_GRENADE", 5);
//DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSurvivedHeadshotDueToHelmet, "SURVIVED_HEADSHOT_DUE_TO_HELMET", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillSniperWithSniper, "KILL_SNIPER_WITH_SNIPER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillSniperWithKnife, "KILL_SNIPER_WITH_KNIFE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSHipShot, "HIP_SHOT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillWhenAtLowHealth, "KILL_WHEN_AT_LOW_HEALTH", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGrenadeMultikill, "GRENADE_MULTIKILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBombMultikill, "BOMB_MULTIKILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSPistolRoundKnifeKill, "PISTOL_ROUND_KNIFE_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFastRoundWin, "FAST_ROUND_WIN", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSurviveManyAttacks, "SURVIVE_MANY_ATTACKS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGooseChase, "GOOSE_CHASE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinBombPlantAfterRecovery, "WIN_BOMB_PLANT_AFTER_RECOVERY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSLosslessExtermination, "LOSSLESS_EXTERMINATION", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFlawlessVictory, "FLAWLESS_VICTORY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinDualDuel, "WIN_DUAL_DUEL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFastHostageRescue, "FAST_HOSTAGE_RESCUE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBreakWindows, "BREAK_WINDOWS", 5);
//DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBreakProps, "BREAK_PROPS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSUnstoppableForce, "UNSTOPPABLE_FORCE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSImmovableObject, "IMMOVABLE_OBJECT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSHeadshotsInRound, "HEADSHOTS_IN_ROUND", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillWhileInAir, "KILL_WHILE_IN_AIR", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyInAir, "KILL_ENEMY_IN_AIR", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillerAndEnemyInAir, "KILLER_AND_ENEMY_IN_AIR", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSilentWin, "SILENT_WIN", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBloodlessVictory, "BLOODLESS_VICTORY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinRoundsWithoutBuying, "WIN_ROUNDS_WITHOUT_BUYING", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDefuseDefense, "DEFUSE_DEFENSE", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillBombPickup, "KILL_BOMB_PICKUP", 5)
#if(ALL_WEARING_SAME_UNIFORM_ACHIEVEMENT)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSameUniform, "SAME_UNIFORM", 5)
#endif
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSConcurrentDominations, "CONCURRENT_DOMINATIONS", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDominationOverkillsMatch, "DOMINATION_OVERKILLS_MATCH", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSExtendedDomination, "EXTENDED_DOMINATION", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSCauseFriendlyFireWithFlashbang, "CAUSE_FRIENDLY_FIRE_WITH_FLASHBANG", 5)

DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameKillKnifer, "GUN_GAME_KILL_KNIFER", 5);
//DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameKnifeSuicide, "GUN_GAME_SELECT_SUICIDE_WITH_KNIFE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameKnifeKillKnifer, "GUN_GAME_KNIFE_KILL_KNIFER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameSMGKillKnifer, "GUN_GAME_SMG_KILL_KNIFER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameProgressiveRampage, "GUN_GAME_RAMPAGE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameFirstKill, "GUN_GAME_FIRST_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyTerrTeamBeforeBombPlant, "GUN_GAME_FIRST_THING_FIRST", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyCTTeamBeforeBombPlant, "GUN_GAME_TARGET_SECURED", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFirstBulletKills, "ONE_SHOT_ONE_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGunGameConservationist, "GUN_GAME_CONSERVATIONIST", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSpawnCamper, "BASE_SCAMPER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBornReady, "BORN_READY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSStillAlive, "STILL_ALIVE", 5);






//-----------------------------------------------------------------------------
// Meta Achievements
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Get all the pistol achievements
//-----------------------------------------------------------------------------
class CAchievementCS_PistolMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_PistolMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsDeagle);
		AddRequirement(CSEnemyKillsGlock);
		AddRequirement(CSEnemyKillsElite);
		AddRequirement(CSEnemyKillsFiveSeven);
		AddRequirement(CSEnemyKillsTec9);
		AddRequirement(CSEnemyKillsHKP2000);
		AddRequirement(CSEnemyKillsP250);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_PistolMaster, CSMetaPistol, "META_PISTOL", 10);

//-----------------------------------------------------------------------------
// Purpose: Get all the rifle achievements
//-----------------------------------------------------------------------------
class CAchievementCS_RifleMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_RifleMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsAWP);
		AddRequirement(CSEnemyKillsAK47);
		AddRequirement(CSEnemyKillsM4A1);
		AddRequirement(CSEnemyKillsAUG);
		AddRequirement(CSEnemyKillsGALILAR);
		AddRequirement(CSEnemyKillsFAMAS);
		AddRequirement(CSEnemyKillsG3SG1);
		AddRequirement(CSEnemyKillsSCAR20);
		AddRequirement(CSEnemyKillsSG556);
		AddRequirement(CSEnemyKillsSSG08);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_RifleMaster, CSMetaRifle, "META_RIFLE", 10);

//-----------------------------------------------------------------------------
// Purpose: Get all the SMG achievements
//-----------------------------------------------------------------------------
class CAchievementCS_SubMachineGunMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_SubMachineGunMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsP90); 
		AddRequirement(CSEnemyKillsMAC10); 
		AddRequirement(CSEnemyKillsUMP45); 
		AddRequirement(CSEnemyKillsMP7); 
		AddRequirement(CSEnemyKillsMP9); 
		AddRequirement(CSEnemyKillsBizon);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_SubMachineGunMaster, CSMetaSMG, "META_SMG", 10);

//-----------------------------------------------------------------------------
// Purpose: Get all the Shotgun achievements
//-----------------------------------------------------------------------------
class CAchievementCS_ShotgunMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_ShotgunMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsXM1014);
		AddRequirement(CSEnemyKillsMag7);
		AddRequirement(CSEnemyKillsSawedoff); 
		AddRequirement(CSEnemyKillsNova);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_ShotgunMaster, CSMetaShotgun, "META_SHOTGUN", 10);

//-----------------------------------------------------------------------------
// Purpose: Get every weapon achievement
//-----------------------------------------------------------------------------
class CAchievementCS_WeaponMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_WeaponMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSMetaPistol);	
		AddRequirement(CSMetaRifle);
		AddRequirement(CSMetaSMG);
		AddRequirement(CSMetaShotgun);
		AddRequirement(CSEnemyKillsM249);
		AddRequirement(CSEnemyKillsNegev);
		AddRequirement(CSEnemyKillsKnife);
		AddRequirement(CSEnemyKillsHEGrenade);
		AddRequirement(CSEnemyKillsMolotov);
		AddRequirement(CSEnemyKillsTaser);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_WeaponMaster, CSMetaWeaponMaster, "META_WEAPONMASTER", 50);


class CAchievementCS_KillWithAllWeapons : public CCSBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	void OnPlayerStatsUpdate( int nUserSlot )
	{
		const StatsCollection_t& stats = g_CSClientGameStats.GetLifetimeStats( nUserSlot );

		static CSStatType_t weaponKillStats[] = {
			CSSTAT_KILLS_DEAGLE,	
			//CSSTAT_KILLS_USP,					
			CSSTAT_KILLS_GLOCK,					
			//CSSTAT_KILLS_P228,		
			CSSTAT_KILLS_ELITE,		
			CSSTAT_KILLS_FIVESEVEN, 
			CSSTAT_KILLS_AWP,		
			CSSTAT_KILLS_AK47,		
			CSSTAT_KILLS_M4A1,		
			CSSTAT_KILLS_AUG,		
			//CSSTAT_KILLS_SG552,		
			//CSSTAT_KILLS_SG550,		
			CSSTAT_KILLS_GALILAR,	
			CSSTAT_KILLS_FAMAS,		
			//CSSTAT_KILLS_SCOUT,		
			CSSTAT_KILLS_G3SG1,		
			CSSTAT_KILLS_P90,		
			//CSSTAT_KILLS_MP5NAVY,	
			//CSSTAT_KILLS_TMP,					
			CSSTAT_KILLS_MAC10,		
			CSSTAT_KILLS_UMP45,		
			//CSSTAT_KILLS_M3,		
			CSSTAT_KILLS_XM1014,	
			CSSTAT_KILLS_M249,		
			//CSSTAT_KILLS_KNIFE,		
			//CSSTAT_KILLS_HEGRENADE, 
			//CSSTAT_KILLS_MOLOTOV,	
			//CSSTAT_KILLS_DECOY,		
			CSSTAT_KILLS_BIZON,		
			CSSTAT_KILLS_MAG7,		
			CSSTAT_KILLS_NEGEV,		
			CSSTAT_KILLS_SAWEDOFF,	
			CSSTAT_KILLS_TEC9,		
			CSSTAT_KILLS_TASER,		
			CSSTAT_KILLS_HKP2000,	
			CSSTAT_KILLS_MP7,		
			CSSTAT_KILLS_MP9,		
			CSSTAT_KILLS_NOVA,		
			CSSTAT_KILLS_P250,		
			//CSSTAT_KILLS_SCAR17,	
			CSSTAT_KILLS_SCAR20,	
			CSSTAT_KILLS_SG556,		
			CSSTAT_KILLS_SSG08,		
		};
		const int numKillStats = ARRAYSIZE( weaponKillStats );

		//Loop through all weapons we care about and make sure we got a kill with each.
		for (int i = 0; i < numKillStats; ++i)
		{

			CSStatType_t statId = weaponKillStats[i];

			if ( stats[statId] == 0)
			{
				return;
			}
		}

		//If we haven't bailed yet, award the achievement.		
		IncrementCount();
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_KillWithAllWeapons, CSKillWithEveryWeapon, "KILL_WITH_EVERY_WEAPON", 5 );

class CAchievementCS_WinEveryGGMap : public CCSBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	void OnPlayerStatsUpdate( int nUserSlot )
	{
		CSStatType_t mapStatsID[] = {
									 CSSTAT_MAP_MATCHES_WON_BAGGAGE,
									 CSSTAT_MAP_MATCHES_WON_LAKE,
									 CSSTAT_MAP_MATCHES_WON_BANK,
									 CSSTAT_MAP_MATCHES_WON_SAFEHOUSE,
									 CSSTAT_MAP_MATCHES_WON_SUGARCANE,
									 CSSTAT_MAP_MATCHES_WON_STMARC,
									 CSSTAT_MAP_MATCHES_WON_SHOOTS,
									 CSSTAT_MAP_MATCHES_WON_SHORTTRAIN,
// DLC									 
//									 CSSTAT_MAP_MATCHES_WON_EMBASSY,
//									 CSSTAT_MAP_MATCHES_WON_DEPOT,
//									 CSSTAT_MAP_MATCHES_WON_MONASTERY,
//									 CSSTAT_MAP_MATCHES_WON_VERTIGO,
//									 CSSTAT_MAP_MATCHES_WON_BALKAN,

									 CSSTAT_MAX}; // just a stat type to indicate our end of list

		const StatsCollection_t& stats = g_CSClientGameStats.GetLifetimeStats( nUserSlot );

		//Loop through all maps we care about and make sure we got a round win
		for (int ii = 0; mapStatsID[ii] != CSSTAT_MAX; ++ii)
		{
			CSStatType_t statId = mapStatsID[ii];

			if ( stats[statId] == 0)
			{
				return;
			}
		}

		//If we haven't bailed yet, award the achievement.  We've won a round in all these levels!
		IncrementCount();
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_WinEveryGGMap, CSWinEveryGGMap, "WIN_EVERY_GUNGAME_MAP", 5 );


class CAchievementCS_PlayEveryGGMap : public CCSBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	void OnPlayerStatsUpdate( int nUserSlot )
	{
		CSStatType_t mapStatsID[] = {CSSTAT_MAP_ROUNDS_AR_BAGGAGE,
			CSSTAT_MAP_ROUNDS_DE_LAKE,
			CSSTAT_MAP_ROUNDS_DE_BANK,
			CSSTAT_MAP_ROUNDS_AR_SHOOTS,
			CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE,
			CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN, 
			CSSTAT_MAP_ROUNDS_DE_SUGARCANE,
			CSSTAT_MAP_ROUNDS_DE_STMARC,
//			CSSTAT_MAP_ROUNDS_DE_DEPOT,
//			CSSTAT_MAP_ROUNDS_DE_EMBASSY,
//			CSSTAT_MAP_ROUNDS_DE_MONASTERY,
			CSSTAT_MAX}; // just a stat type to indicate our end of list

		const StatsCollection_t& stats = g_CSClientGameStats.GetLifetimeStats( nUserSlot );

		//Loop through all maps we care about and make sure we got a round win
		for (int ii = 0; mapStatsID[ii] != CSSTAT_MAX; ++ii)
		{
			CSStatType_t statId = mapStatsID[ii];

			if ( stats[statId] == 0)
			{
				return;
			}
		}

		//If we haven't bailed yet, award the achievement.  We've won a round in all these levels!
		IncrementCount();
	}
};

DECLARE_ACHIEVEMENT( CAchievementCS_PlayEveryGGMap, CSPlayEveryGGMap, "PLAY_EVERY_GUNGAME_MAP", 5 );

#if(ALL_WEARING_SAME_UNIFORM_ACHIEVEMENT)
class CAchievementCS_FriendsSameUniform : public CCSBaseAchievement
{
	void Init()
	{
		SetFlags(ACH_SAVE_GLOBAL);
		SetGoal(1);
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "round_start" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( Q_strcmp( event->GetName(), "round_start" ) == 0 )
		{
			int localPlayerIndex = GetLocalPlayerIndex();
			C_CSPlayer* pLocalPlayer = ToCSPlayer(UTIL_PlayerByIndex(localPlayerIndex));

			// Initialize all to 1, since the local player doesn't get counted as we loop.
			int numPlayersOnTeam = 1;
			int numFriendsOnTeam = 1;
			int numMatchingFriendsOnTeam = 1;

			if (pLocalPlayer)
			{    
				int localPlayerClass = pLocalPlayer->PlayerClass();
				int localPlayerTeam = pLocalPlayer->GetTeamNumber();
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					if ( i != localPlayerIndex)
					{
						CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

						if (pPlayer)
						{
							if (pPlayer->GetTeamNumber() == localPlayerTeam)
							{
								++numPlayersOnTeam;
#if defined (_X360)
								ACTIVE_SPLITSCREEN_PLAYER_GUARD( pLocalPlayer->GetSplitScreenPlayerSlot() );
								if ( IsXboxFriends( XBX_GetActiveUserId(), pPlayer->entindex() ) )
#else
								if ( pLocalPlayer->HasPlayerAsFriend( pPlayer ) )
#endif
								{
									++numFriendsOnTeam;
									if ( pPlayer->PlayerClass() == localPlayerClass )
										++numMatchingFriendsOnTeam;
								}
							}
						}
					}
				}

				if (numMatchingFriendsOnTeam >= AchievementConsts::FriendsSameUniform_MinPlayers )
				{
					AwardAchievement();
				}
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_FriendsSameUniform, CSFriendsSameUniform, "FRIENDS_SAME_UNIFORM", 5 );
#endif // #if(ALL_WEARING_SAME_UNIFORM_ACHIEVEMENT)


class CAchievementCS_AvengeFriend : public CCSBaseAchievement
{
	void Init()
	{
		SetFlags(ACH_SAVE_GLOBAL);
		SetGoal(1);
	}

	void ListenForEvents()
	{
		ListenForGameEvent( "player_avenged_teammate" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( Q_strcmp( event->GetName(), "player_avenged_teammate" ) == 0 )
		{
			int localPlayerIndex = GetLocalPlayerIndex();
			C_CSPlayer* pLocalPlayer = ToCSPlayer(UTIL_PlayerByIndex(localPlayerIndex));

			//for debugging
			//int eventId = event->GetInt( "avenger_id" );
			//int localUserId = pLocalPlayer->GetUserID();

			if (pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "avenger_id" ))
			{
				int avengedPlayerIndex = engine->GetPlayerForUserID( event->GetInt( "avenged_player_id" ) );

				if ( avengedPlayerIndex > 0 )
				{
					C_CSPlayer* pAvengedPlayer = ToCSPlayer( UTIL_PlayerByIndex( avengedPlayerIndex ) );
					C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
					if ( cs_PR && pAvengedPlayer )
					{
						XUID nOtherXUID = cs_PR->GetXuid( pAvengedPlayer->entindex() );
						if ( g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetFriendByXUID( nOtherXUID ) )
						{
							AwardAchievement();
						}
					}
				}             
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_AvengeFriend, CSAvengeFriend, "AVENGE_FRIEND", 5 );



class CAchievementCS_Medalist : public CCSBaseAchievement
{
	void Init()
	{
		SetFlags(ACH_SAVE_GLOBAL);
		SetGoal(1);
	}

	void ListenForEvents()
	{
		ListenForGameEvent( "achievement_earned_local" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( Q_strcmp( event->GetName(), "achievement_earned_local" ) == 0 )
		{
			int splitScreenPlayer = event->GetInt( "splitscreenplayer" );

			int totalEarnedAchievements = 0;
			FOR_EACH_MAP( g_AchievementMgrCS.GetAchievements(splitScreenPlayer), i )
			{
				CBaseAchievement *pAchievement = g_AchievementMgrCS.GetAchievements( splitScreenPlayer )[i];
				if ( pAchievement && pAchievement->IsAchieved() )
					totalEarnedAchievements++;
			}

			if ( totalEarnedAchievements >= AchievementConsts::Num_Medalist_Required_Medals)
			{
				AwardAchievement();
			}

		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_Medalist, CSMedalist, "MEDALIST", 5 );

#if defined ( _X360 )
static CAchievementListener g_AchievementListener;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAchievementListener::CAchievementListener() :
CAutoGameSystem( "CAchievementListener" )
{

}

//-----------------------------------------------------------------------------
// Purpose: Listen for earned achievements
//-----------------------------------------------------------------------------
bool CAchievementListener::Init()
{
	ListenForGameEvent( "achievement_earned_local" );
	ListenForGameEvent( "repost_xbox_achievements" );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Listens for game events.  Send off XBLA achievements for CS
//-----------------------------------------------------------------------------
void CAchievementListener::FireGameEvent( IGameEvent *event )
{
	const char *eventname = event->GetName();

	XNotifyPositionUI( XNOTIFYUI_POS_TOPCENTER );

	if ( Q_strcmp( "achievement_earned_local", eventname ) == 0 )
	{
		// is this one of our XBox achievements?
		int achievementID = event->GetInt( "achievement" );
		int splitScreenPlayer = event->GetInt( "splitscreenplayer" );
		for ( int i=0; i<NumXboxMappedAchievements; ++i )
		{
			if ( MedalToXBox[i][0] == achievementID )
			{
				// award the Xbox achievement; pass CSInvalidAchievement in case the overlappedresult from the xbox achievement award is invalid we don't
				// unaward a medal in achievementmgr::Update loop where the overlapped results are processed
				g_AchievementMgrCS.AwardXBoxAchievement( CSInvalidAchievement, MedalToXBox[i][1], splitScreenPlayer );
			}
		}

		// have we earned the required # of Medals to trigger the MEDALIST xBox achievement?
		int totalEarnedAchievements = 0;
		FOR_EACH_MAP( g_AchievementMgrCS.GetAchievements(splitScreenPlayer), i )
		{
			CBaseAchievement *pAchievement = g_AchievementMgrCS.GetAchievements( splitScreenPlayer )[i];
			if ( pAchievement && pAchievement->IsAchieved() )
				totalEarnedAchievements++;
		}
		if ( totalEarnedAchievements >= AchievementConsts::Num_Medalist_Required_Medals )
		{
			// award the Xbox achievement; pass CSInvalidAchievement in case the overlappedresult from the xbox achievement award is invalid we don't
			// unaward a medal in achievementmgr::Update loop where the overlapped results are processed
			g_AchievementMgrCS.AwardXBoxAchievement( CSInvalidAchievement, ACHIEVEMENT_MEDALIST, splitScreenPlayer );
		}

	}
	else if ( Q_strcmp( "repost_xbox_achievements", eventname ) == 0 )
	{
		// check to see if any medals have already been earned that would award an xBox achievement; it is ok to try to award an already awarded xbox achievement
		// and this makes sure we award during a profile read any achievements the player earned that did not properly get awarded for some reason
		int splitScreenPlayer = event->GetInt( "splitscreenplayer" );
		
		for ( int i=0; i<NumXboxMappedAchievements; ++i) 
		{
			CBaseAchievement *pAchievement = g_AchievementMgrCS.GetAchievementByID( MedalToXBox[i][0], splitScreenPlayer );
			if ( pAchievement && pAchievement->IsAchieved() )
			{
				// award the Xbox achievement; pass CSInvalidAchievement in case the overlappedresult from the xbox achievement award is invalid we don't
				// unaward a medal in achievementmgr::Update loop where the overlapped results are processed
				g_AchievementMgrCS.AwardXBoxAchievement( CSInvalidAchievement, MedalToXBox[i][1], splitScreenPlayer );
			}
		}

		// have we earned the required # of Medals to trigger the MEDALIST xBox achievement?
		int totalEarnedAchievements = 0;
		FOR_EACH_MAP( g_AchievementMgrCS.GetAchievements(splitScreenPlayer), i )
		{
			CBaseAchievement *pAchievement = g_AchievementMgrCS.GetAchievements( splitScreenPlayer )[i];
			if ( pAchievement && pAchievement->IsAchieved() )
				totalEarnedAchievements++;
		}
		if ( totalEarnedAchievements >= AchievementConsts::Num_Medalist_Required_Medals )
		{
			// award the Xbox achievement; pass CSInvalidAchievement in case the overlappedresult from the xbox achievement award is invalid we don't
			// unaward a medal in achievementmgr::Update loop where the overlapped results are processed
			g_AchievementMgrCS.AwardXBoxAchievement( CSInvalidAchievement, ACHIEVEMENT_MEDALIST, splitScreenPlayer );
		}
	}
}

#endif // (_X360)


#endif // CLIENT_DLL

