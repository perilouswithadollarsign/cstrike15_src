#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_gamestats.h"
#include "funfactmgr_cs.h"
#include "funfact_cs.h"
#include "weapon_csbase.h"
#include "cs_achievement_constants.h"

const float kFirstBloodTimeClassic		= 30.0f;
const float kFirstKillTimeClassic		= 45.0f;
const float kFirstBloodTimeDemolition	= 15.0f;
const float kFirstKillTimeDemolition	= 25.0f;
const float kFastRoundTimeClassic		= 40.0f;
const float kFastRoundTimeElimination	= 30.0f;	// used for demolition fast win fun facts
const int	kMinShotsForAccuracy		= 15;		// number of shots required for accuracy to be considered valid
const float kAccuracyAdvantage			= 0.25f;	// delta required for player > team
const float kMinTeamAccuracy			= 0.30f;	// accuracy required for team fun facts

enum FunFactId
{
	FF_CT_WIN_NO_KILLS,
	FF_T_WIN_NO_KILLS,
	FF_KILL_DEFUSER,
	FF_KILL_RESCUER,
	FF_T_WIN_NO_CASUALTIES,
	FF_CT_WIN_NO_CASUALTIES,
	FF_DAMAGE_WITH_GRENADES,
	FF_KILLS_WITH_GRENADES,
	FF_SINGLE_GRENADE_KILLS,
	FF_DAMAGE_NO_KILLS,
	FF_KILLED_ENEMIES,
	FF_FIRST_KILL,
	FF_FIRST_BLOOD,
	FF_SHORT_ROUND,
	FF_BEST_ACCURACY,
	FF_KNIFE_KILLS,
	FF_BLIND_KILLS,
	FF_KILLS_WITH_LAST_ROUND,
	FF_DONATED_WEAPONS,
	FF_POSTHUMOUS_GRENADE_KILLS,    
	FF_KNIFE_IN_GUNFIGHT,
	FF_NUM_TIMES_JUMPED,
	FF_FALL_DAMAGE,
	FF_ITEMS_PURCHASED,
	FF_WON_AS_LAST_MEMBER,
	FF_NUMBER_OF_OVERKILLS,
	FF_SHOTS_FIRED,
	FF_SHOTS_FIRED_GG,
	FF_MONEY_SPENT,
	FF_MULTIPLE_ATTACKS_LIVED,
	FF_MULTIPLE_ATTACKS_DIED,
	FF_DAMAGE_MULTIPLE_ENEMIES,
	FF_GRENADES_THROWN,
	FF_USED_ALL_AMMO,
	FF_DEFENDED_BOMB,
	FF_ITEMS_DROPPED_VALUE,
	FF_KILL_WOUNDED_ENEMIES,
	FF_USED_MULTIPLE_WEAPONS,
	FF_T_ACCURACY,
	FF_CT_ACCURACY,
	FF_BEST_T_ACCURACY,
	FF_BEST_CT_ACCURACY,
	FF_KILLS_HEADSHOTS,
	FF_KILLS_WITH_STATTRAK_WEAPON,
	FF_BROKE_WINDOWS,
	FF_NIGHTVISION_DAMAGE,
    FF_DEFUSED_WITH_DROPPED_KIT,
    FF_KILLED_HALF_OF_ENEMIES,
    FF_KILLED_ALL_ENEMIES,
	FF_KNIFE_LEVEL_REACHED,
	FF_MAX_KILLED_BEFORE_DYING,
	FF_MAX_RESPAWNS,
	FF_DEFAULT_WEAPON,
	FF_ROUNDS_WITHOUT_DYING,
	FF_TASER_KILL,
	FF_TICKING_TIME,
	FF_CT_WIN_TIME,
	FF_TER_WIN_TIME,
	FF_BOTS_ASSUMED,
	FF_DOMINATION,
	FF_REVENGE,
	FF_STEPS_TAKEN,
	FF_QUARTER_HEALTH,
	FF_EMPTY_GUNS,
	FF_SLOW_TRIGGER,
	FF_PICKUP_BOMB,
	FF_BOMB_CARRIERS,
	FF_KNIFE_BOMB_PLANTER,
	FF_BOMB_PLANTED_BEFORE_KILL,
	FF_FAILED_BOMB_PLANTS,
	FF_KNIFE_WITHOUT_AMMO,
	FF_MOLOTOV_BURNS,
	FF_SURVIVAL_TIME,
	FF_PULLED_TRIGGER,
	FF_DEFUSE_WAS_CLOSE_CALL_TENTHS,
	FF_DEFUSE_WAS_CLOSE_CALL_HUNDREDTHS,
	FF_DEFUSE_WAS_CLOSE_CALL_THOUSANDTHS,
	FF_FALLBACK,
};


CFunFactHelper *CFunFactHelper::s_pFirst = NULL;



//=============================================================================
// Per-player evaluation Fun Fact
// Evaluate the function per player and generate a fun fact for each valid or
// highest valid player
//=============================================================================

namespace GameFlags
{
	enum Type
	{
		None		= 0x0000,
		Classic		= 0x0001,
		Demolition	= 0x0002,
		GunGame		= 0x0004,
		AllModes	= 0x0007,
		NotGunGame	= AllModes & ~GunGame,
		Elimination	= 0x0100,			// win by elimination
	};
};

bool GameQualifies( e_RoundEndReason roundResult, int gameFlags )
{
	if ( gameFlags & GameFlags::Elimination )
	{
		if ( roundResult != CTs_Win && roundResult != Terrorists_Win )
			return false;
	}

	// check to see if we're enabled to run in our game mode
	if ( (gameFlags & GameFlags::Classic) && CSGameRules()->IsPlayingClassic() )
		return true;
	if ( (gameFlags & GameFlags::Demolition) && CSGameRules()->IsPlayingGunGameTRBomb() )
		return true;
	if ( (gameFlags & GameFlags::GunGame) && CSGameRules()->IsPlayingGunGameProgressive() )
		return true;

	return false;
}


//=============================================================================
// Generic evaluation Fun Fact
// This fun fact will evaluate the specified function to determine when it is
// valid. This is basically just a glue class for simple evaluation functions.
//=============================================================================

// Function type that we use to evaluate our fun facts.  The data is returned as ints then floats that are passed in as reference parameters
typedef bool (*fFunFactEval)( int &iPlayer, int &data1, int &data2, int &data3 );

class CFunFact_GenericEvalFunction : public FunFactEvaluator
{
public:
	CFunFact_GenericEvalFunction(FunFactId id, const char* szLocalizationToken, float fCoolness, fFunFactEval pfnEval, int gameFlags ) :
		FunFactEvaluator(id, szLocalizationToken, fCoolness),
		m_pfnEval(pfnEval),
		m_gameFlags(gameFlags)
		{}

	virtual bool Evaluate( e_RoundEndReason roundResult, FunFactVector& results ) const
	{
		if ( !GameQualifies(roundResult, m_gameFlags) )
			return false;

		FunFact funfact;
		if (m_pfnEval(funfact.iPlayer, funfact.iData1, funfact.iData2, funfact.iData3))
		{
			funfact.id = GetId();
			funfact.szLocalizationToken = GetLocalizationToken();
			funfact.fMagnitude = 0.0f;
			results.AddToTail(funfact);
			return true;
		}
		else
			return false;
	}

	private:
		fFunFactEval	m_pfnEval;
		int				m_gameFlags;
};
#define DECLARE_FUNFACT_EVALFUNC(funfactId, szLocalizationToken, fCoolness, pfnEval, gameFlags)				\
static FunFactEvaluator *CreateFunFact_##funfactId( void )													\
{																											\
	return new CFunFact_GenericEvalFunction(funfactId, szLocalizationToken, fCoolness, pfnEval, gameFlags);	\
};																											\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// Per-player evaluation Fun Fact
// Evaluate the function per player and generate a fun fact for each valid or
// highest valid player
//=============================================================================

namespace PlayerFlags
{
	enum Type
	{
		All						= 0x0000,
		TeamCT					= 0x0001,
		TeamTerrorist			= 0x0002,
		HighestOnly				= 0x0004,	// when not set, generates fun facts for all valid testees
		Alive					= 0x0008,
		Dead					= 0x0010,
		WinningTeam				= 0x0020,
		LosingTeam				= 0x0040,
		HasKilledThisRound		= 0x0080, 
		AllowIfControlledBot	= 0x0100,	//  allow if this player controlled a bot this round

	};
};


bool PlayerQualifies( CBasePlayer* pPlayer, int playerFlags )
{
	if ( (playerFlags & PlayerFlags::TeamCT) && pPlayer->GetTeamNumber() != TEAM_CT )
		return false;
	if ( (playerFlags & PlayerFlags::TeamTerrorist) && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
		return false;

	if ( !pPlayer )
		return false;

	CCSPlayer* pCSPlayer = ToCSPlayer(pPlayer);
	if( pCSPlayer && pCSPlayer->IsBot() && pCSPlayer->HasBeenControlledThisRound() )
		return false;  // bots are not allowed to be picked after being controlled

	if ( !( playerFlags & PlayerFlags::AllowIfControlledBot ) && pCSPlayer && pCSPlayer->HasControlledBotThisRound() )
		return false;

	if( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return false;
	if ( (playerFlags & PlayerFlags::Dead) && const_cast<CBasePlayer*>(pPlayer)->IsAlive() )	// IsAlive() really isn't const correct
		return false;
	if ( (playerFlags & PlayerFlags::Alive) && !const_cast<CBasePlayer*>(pPlayer)->IsAlive() )
		return false;
	if ( (playerFlags & PlayerFlags::WinningTeam) && pPlayer->GetTeamNumber() != CSGameRules()->m_iRoundWinStatus )
		return false;
	if ( (playerFlags & PlayerFlags::LosingTeam) && pPlayer->GetTeamNumber() == CSGameRules()->m_iRoundWinStatus )
		return false;
	if ( ( playerFlags & PlayerFlags::HasKilledThisRound ) && CCS_GameStats.FindPlayerStats( pPlayer ).statsCurrentRound[CSSTAT_KILLS] == 0 )
		return false;

	return true;
}


typedef int (*PlayerEvalFunction)(CCSPlayer* pPlayer);

class CFunFact_PlayerEvalFunction : public FunFactEvaluator
{
public:
	CFunFact_PlayerEvalFunction(FunFactId id, const char* szLocalizationToken, float fCoolness, PlayerEvalFunction pfnEval, 
		int iMin, int gameFlags, int playerFlags ) : 
	FunFactEvaluator(id, szLocalizationToken, fCoolness),
		m_pfnEval(pfnEval),
		m_min(iMin),
		m_gameFlags(gameFlags),
		m_playerFlags(playerFlags)
	{}

	virtual bool Evaluate( e_RoundEndReason roundResult, FunFactVector& results ) const
	{
		if ( !GameQualifies(roundResult, m_gameFlags) )
			return false;

		int iBestValue = 0;
		int iBestPlayer = 0;
		bool bResult = false;
		bool bTied = false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer* pPlayer = ToCSPlayer(UTIL_PlayerByIndex( i ) );
			if ( pPlayer )
			{
				if (!PlayerQualifies(pPlayer, m_playerFlags))
					continue;

				int iValue = m_pfnEval(pPlayer);

				if (m_playerFlags & PlayerFlags::HighestOnly)
				{
					if ( iValue > iBestValue )
					{
						iBestValue = iValue;
						iBestPlayer = i;
						bTied = false;
					}
					else if ( iValue == iBestValue )
					{
						bTied = true;
					}
				}
				else
				{
					// generate fun facts for any player who meets the validation requirement
					if ( iValue >= m_min )
					{
						FunFact funfact;
						funfact.id = GetId();
						funfact.szLocalizationToken = GetLocalizationToken();
						funfact.iPlayer = i;
						funfact.iData1 = iValue;
						funfact.fMagnitude = 1.0f - ((float)m_min / iValue);
						results.AddToTail(funfact);
						bResult = true;
					}
				}
			}
		}
		if ( (m_playerFlags & PlayerFlags::HighestOnly) && iBestValue >= m_min && !bTied )
		{
			FunFact funfact;
			funfact.id = GetId();
			funfact.szLocalizationToken = GetLocalizationToken();
			funfact.iPlayer = iBestPlayer;
			funfact.iData1 = iBestValue;
			funfact.fMagnitude = 1.0f - ((float)m_min / iBestValue);

			results.AddToTail(funfact);
			bResult = true;
		}
		return bResult;
	}

private:
	PlayerEvalFunction	m_pfnEval;
	int					m_min;
	int					m_gameFlags;
	int					m_playerFlags;
};
#define DECLARE_FUNFACT_PLAYERFUNC(funfactId, szLocalizationToken, fCoolness, pfnEval, iMin, gameFlags, playerFlags )			\
static FunFactEvaluator *CreateFunFact_##funfactId( void )																		\
{																																\
	return new CFunFact_PlayerEvalFunction(funfactId, szLocalizationToken, fCoolness, pfnEval,	iMin, gameFlags, playerFlags);	\
};																																\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// Per-team evaluation Fun Fact
//=============================================================================

typedef bool (*TeamEvalFunction)(int iTeam, int &data1, int &data2, int &data3);

class CFunFact_TeamEvalFunction : public FunFactEvaluator
{
public:
	CFunFact_TeamEvalFunction(FunFactId id, const char* szLocalizationToken, float fCoolness, TeamEvalFunction pfnEval, int iTeam, int gameFlags ) : 
	  FunFactEvaluator(id, szLocalizationToken, fCoolness),
		  m_pfnEval(pfnEval),
		  m_team(iTeam),
		  m_gameFlags(gameFlags)
	  {}

	virtual bool Evaluate( e_RoundEndReason roundResult, FunFactVector& results ) const
	{
		if ( !GameQualifies(roundResult, m_gameFlags) )
			return false;

		int iData1, iData2, iData3;
		if ( m_pfnEval(m_team, iData1, iData2, iData3) )
		{
			FunFact funfact;
			funfact.id = GetId();
			funfact.szLocalizationToken = GetLocalizationToken();
			funfact.fMagnitude = 0.0f;
			results.AddToTail(funfact);
			return true;
		}
		return false;
	}

private:
	TeamEvalFunction m_pfnEval;
	int					m_team;
	int					m_gameFlags;
};
#define DECLARE_FUNFACT_TEAMFUNC(funfactId, szLocalizationToken, fCoolness, pfnEval, iTeam, gameFlags)			\
static FunFactEvaluator *CreateFunFact_##funfactId( void )														\
{																												\
	return new CFunFact_TeamEvalFunction(funfactId, szLocalizationToken, fCoolness, pfnEval, iTeam, gameFlags);	\
};																												\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// High Stat-based Fun Fact
// This fun fact will find the player with the highest value for a particular
// stat, and validate when that stat exceeds a specified minimum
//=============================================================================
class CFunFact_StatBest : public FunFactEvaluator
{
public:
	CFunFact_StatBest(FunFactId id, const char* szLocalizationToken, float fCoolness, CSStatType_t statId, int iMin, int gameFlags, int playerFlags ) :
	  FunFactEvaluator(id, szLocalizationToken, fCoolness),
		m_statId(statId),
		m_min(iMin),
		m_gameFlags(gameFlags),
		m_playerFlags(playerFlags)
	{
		V_strncpy(m_singularLocalizationToken, szLocalizationToken, sizeof(m_singularLocalizationToken));
		if (m_min == 1)
		{
			V_strncat(m_singularLocalizationToken, "_singular", sizeof(m_singularLocalizationToken));
		}
	}

	virtual bool Evaluate( e_RoundEndReason roundResult, FunFactVector& results ) const
	{
		if ( !GameQualifies(roundResult, m_gameFlags) )
			return false;

		int iBestValue = 0;
		int iBestPlayer = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				if ( !PlayerQualifies(pPlayer, m_playerFlags) )
					continue;

				int iValue = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[m_statId];
				if ( iValue > iBestValue )
				{
					iBestValue = iValue;
					iBestPlayer = i;
				}
			}
		}
		if ( iBestValue >= m_min )
		{
			FunFact funfact;
			funfact.id = GetId();
			funfact.szLocalizationToken = iBestValue == 1 ? m_singularLocalizationToken : GetLocalizationToken();
			funfact.iPlayer = iBestPlayer;
			funfact.iData1 = iBestValue;
			funfact.fMagnitude = 1.0f - ((float)m_min / iBestValue);

			results.AddToTail(funfact);
			return true;
		}
		return false;
	}

private:
	CSStatType_t	m_statId;
	int				m_min;
	char			m_singularLocalizationToken[128];
	int				m_gameFlags;
	int				m_playerFlags;

};
#define DECLARE_FUNFACT_STATBEST(funfactId, szLocalizationToken, fCoolness, statId, iMin, gameFlags, playerFlags)	\
static FunFactEvaluator *CreateFunFact_##funfactId( void )															\
{																													\
	return new CFunFact_StatBest(funfactId, szLocalizationToken, fCoolness, statId, iMin, gameFlags, playerFlags);	\
};																													\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );


//=============================================================================
// Sum-based Fun Fact
// This fun fact will add up a stat for all players, and is valid when the 
// sum exceeds a threshold
//=============================================================================
class CFunFact_StatSum : public FunFactEvaluator
{
public:
	CFunFact_StatSum(FunFactId id, const char* szLocalizationToken, float fCoolness, CSStatType_t statId, int iMin, int gameFlags, int playerFlags ) :
	  FunFactEvaluator(id, szLocalizationToken, fCoolness),
		m_statId(statId),
		m_min(iMin),
		m_gameFlags(gameFlags),
		m_playerFlags(playerFlags)
	{}

	virtual bool Evaluate( e_RoundEndReason roundResult, FunFactVector& results ) const
	{
		if ( !GameQualifies(roundResult, m_gameFlags) )
			return false;

		int iSum = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				if (!PlayerQualifies(pPlayer, m_playerFlags))
					continue; 

				iSum += CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[m_statId];
			}
		}
		if ( iSum >= m_min )
		{
			FunFact funfact;
			funfact.id = GetId();
			funfact.szLocalizationToken = GetLocalizationToken();
			funfact.iPlayer = 0;
			funfact.iData1 = iSum;
			funfact.fMagnitude = 1.0f - ((float)m_min / iSum);

			results.AddToTail(funfact);
			return true;
		}
		return false;
	}

private:
	CSStatType_t	m_statId;
	int				m_min;
	int				m_gameFlags;
	int				m_playerFlags;
};
#define DECLARE_FUNFACT_STATSUM(funfactId, szLocalizationToken, fCoolness, statId, iMin, gameFlags, playerFlags)	\
static FunFactEvaluator *CreateFunFact_##funfactId( void )															\
{																													\
	return new CFunFact_StatSum(funfactId, szLocalizationToken, fCoolness, statId, iMin, gameFlags, playerFlags);	\
};																													\
static CFunFactHelper g_##funfactId##_Helper( CreateFunFact_##funfactId );



//=============================================================================
// Helper function to calculate team accuracy
//=============================================================================

float GetTeamAccuracy( int teamNumber )
{
	int teamShots = 0;
	int teamHits = 0;

	//Add up hits and shots
	CBasePlayer *pPlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if (pPlayer)
		{
			if (pPlayer->GetTeamNumber() == teamNumber)
			{
				teamShots += CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
				teamHits += CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
			}
		}
	}

	if (teamShots > kMinShotsForAccuracy)
		return (float)teamHits / teamShots;

	return 0.0f;
}



//=============================================================================
// fun fact explicit evaluation functions
//=============================================================================

bool FFEVAL_ALWAYS_TRUE( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return true;
}

bool FFEVAL_CT_WIN_NO_KILLS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_CT && CSGameRules()->m_bNoTerroristsKilled );
}

bool FFEVAL_T_WIN_NO_KILLS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_TER && CSGameRules()->m_bNoCTsKilled );
}

bool FFEVAL_T_WIN_NO_CASUALTIES( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_TER && CSGameRules()->m_bNoTerroristsKilled );
}

bool FFEVAL_CT_WIN_NO_CASUALTIES( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return ( CSGameRules()->m_iRoundWinStatus == WINNER_CT && CSGameRules()->m_bNoCTsKilled );
}

int FFEVAL_KILLED_DEFUSER( CCSPlayer* pPlayer )
{
	return pPlayer->GetKilledDefuser() ? 1 : 0;
}

int FFEVAL_KILLED_RESCUER( CCSPlayer* pPlayer )
{
	return pPlayer->GetKilledRescuer() ? 1 : 0;
}

int FFEVAL_KILLS_SINGLE_GRENADE( CCSPlayer* pPlayer )
{
	return pPlayer->GetMaxGrenadeKills();
}

int FFEVAL_DAMAGE_NO_KILLS( CCSPlayer* pPlayer )
{
	if (CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_KILLS] == 0)
		return CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_DAMAGE];
	else
		return 0;
}

bool FFEVAL_FIRST_KILL( int &iPlayer, int &data1, int &data2, int &data3 )
{
	const int timeRequired = CSGameRules()->IsPlayingClassic() ? kFirstKillTimeClassic : kFirstKillTimeDemolition;
	if ( CSGameRules()->m_pFirstKill != NULL && CSGameRules()->m_firstKillTime <= timeRequired )
	{
		iPlayer = CSGameRules()->m_pFirstKill->entindex();
		data1 = CSGameRules()->m_firstKillTime;
		return true;
	}
	return false;
}

bool FFEVAL_FIRST_BLOOD( int &iPlayer, int &data1, int &data2, int &data3 )
{
	const int timeRequired = CSGameRules()->IsPlayingClassic() ? kFirstBloodTimeClassic : kFirstBloodTimeDemolition;
	if ( CSGameRules()->m_pFirstBlood != NULL && CSGameRules()->m_firstBloodTime < timeRequired )
	{
		iPlayer = CSGameRules()->m_pFirstBlood->entindex();
		data1 = CSGameRules()->m_firstKillTime;
		return true;
	}
	return false;
}

bool FFEVAL_CT_WIN_TIME( int &iPlayer, int &data1, int &data2, int &data3 )
{
	if ( CSGameRules()->GetRoundElapsedTime() < kFastRoundTimeElimination && CSGameRules()->m_iRoundWinStatus == WINNER_CT )
	{
		data1 = CSGameRules()->GetRoundElapsedTime();
		return true;
	}
	return false;
}


bool FFEVAL_TER_WIN_TIME( int &iPlayer, int &data1, int &data2, int &data3 )
{
	if ( CSGameRules()->GetRoundElapsedTime() < kFastRoundTimeElimination && CSGameRules()->m_iRoundWinStatus == WINNER_TER )
	{
		data1 = CSGameRules()->GetRoundElapsedTime();
		return true;
	}
	return false;
}

bool FFEVAL_SHORT_ROUND( int &iPlayer, int &data1, int &data2, int &data3 )
{
	if ( CSGameRules()->GetRoundElapsedTime() < kFastRoundTimeClassic )
	{
		data1 = CSGameRules()->GetRoundElapsedTime();
		return true;
	}
	return false;
}

int FFEVAL_ACCURACY( CCSPlayer* pPlayer )
{
	float shots = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
	float hits = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
	if (shots >= kMinShotsForAccuracy)
		return RoundFloatToInt(100.0f * hits / shots);
	return 0;
}

int FFEVAL_EMPTY_GUNS( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumFirearmsRanOutOfAmmo();
}

int FFEVAL_QUARTER_HEALTH( CCSPlayer* pPlayer )
{
	return pPlayer->GetMediumHealthKills();
}

int FFEVAL_STEPS_TAKEN( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumFootsteps();
}
	
int FFEVAL_MOST_CONCURRENT_DOMINATIONS( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumConcurrentDominations();
}

	
int FFEVAL_MOST_BOTS_ASSUMED( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumBotsControlled();
}


int FFEVAL_HIGHEST_PROXIMITY_SCORE( CCSPlayer* pPlayer )
{
	return pPlayer->GetRoundProximityScore();
}

int FFEVAL_ROUNDS_WITHOUT_DYING( CCSPlayer* pPlayer )
{
	return pPlayer->GetCurNumRoundsSurvived();
}

int FFEVAL_DEFAULT_WEAPON( CCSPlayer* pPlayer )
{
	if ( CSGameRules()->IsPistolRound() )
		return 0;

	return pPlayer->GetPickedUpWeaponThisRound() ? 0 : 1;
}

int FFEVAL_KILLED_PERCENT_OF_ENEMIES( CCSPlayer* pPlayer )
{
    return pPlayer->GetPercentageOfEnemyTeamKilled();
}

int FFEVAL_MAX_NUM_RESPAWNS( CCSPlayer* pPlayer )
{
	return pPlayer->m_iNumSpawns;
}

int FFEVAL_KILL_STREAK_BEFORE_DYING( CCSPlayer* pPlayer )
{
	return pPlayer->m_maxNumEnemiesKillStreak;
}

void GetTeamRoundScore( int teamNumber, int* num_players,int* contribution_sum )
{
	CBasePlayer *basePlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		basePlayer = UTIL_PlayerByIndex( i );
		CCSPlayer* pPlayer = ToCSPlayer( basePlayer );

		if( pPlayer && pPlayer->GetTeamNumber() == teamNumber )
		{
			(*num_players)++;
			(*contribution_sum) += pPlayer->GetRoundContributionScore();
		}
	}
}




bool FFEVAL_FAILED_BOMB_PLANTS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	data1 = 0;
	
	CBasePlayer *basePlayer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		basePlayer = UTIL_PlayerByIndex( i );
		CCSPlayer* pPlayer = ToCSPlayer( basePlayer );

		if ( pPlayer && 
			!(pPlayer->GetBombPlacedTime() >= 0.0f) && 
			pPlayer->HasAttemptedBombPlace() )
		{
			iPlayer = i;
			data1++;
		}
	}

	if ( data1 >= 2 )
	{
		return true;
	}


	return false;
}

bool FFEVAL_BOMB_PLANTED_BEFORE_KILL( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float fBombPlantedTime = -1.0f;
	float fFirstKillTime = -1.0f;
	CBasePlayer *basePlayer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		basePlayer = UTIL_PlayerByIndex( i );
		CCSPlayer* pPlayer = ToCSPlayer( basePlayer );

		if ( pPlayer && 
			pPlayer->GetBombPlacedTime() >= 0.0f && 
			( pPlayer->GetBombPlacedTime() <= fBombPlantedTime || fBombPlantedTime < 0.0f ) )
		{
			iPlayer = i;
			fBombPlantedTime = pPlayer->GetBombPlacedTime();
			data1 = (int) fBombPlantedTime;
		}

		if (pPlayer &&
			pPlayer->GetKilledTime() > 0.0f &&
			(fFirstKillTime < 0.0f || pPlayer->GetKilledTime() < fFirstKillTime ) )
		{
			fFirstKillTime = pPlayer->GetKilledTime();
			data2 = (int) fFirstKillTime;
		}
	}


	if( fBombPlantedTime > 0.0f && 
	  (fFirstKillTime <= 0.0f  || fFirstKillTime > fBombPlantedTime ) )
	{
		return true;
	}


	return false;
}

bool FFEVAL_PICKUP_BOMB( int &iPlayer, int &data1, int &data2, int &data3 )
{
	// count number of picker-upers as data1
	data1 = 0;
	CBasePlayer *basePlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		basePlayer = UTIL_PlayerByIndex( i );
		CCSPlayer* pPlayer = ToCSPlayer( basePlayer );

		if( pPlayer && pPlayer->GetBombPickuptime() >= 0.0f )
			data1++;
	}

	if( data1 > 2 ) // enough guys picked up the bomb to be interesting
	{
		// find someone who placed a bomb
		CBasePlayer *basePlayer = NULL;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			basePlayer = UTIL_PlayerByIndex( i );
			CCSPlayer* pPlayer = ToCSPlayer( basePlayer );

			if( pPlayer && pPlayer->GetBombPlacedTime() >= 0.0f )
			{
				data1--;
				iPlayer = i;
				return true;
			}
		}
	}

	return false;
}

bool FFEVAL_TICKING_TIME( int &iPlayer, int &data1, int &data2, int &data3 )
{
	int winningTeam = CSGameRules()->m_iRoundWinStatus;
	if ( winningTeam != TEAM_TERRORIST )
		return false;

	data1 = 0;
	CBasePlayer *basePlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		basePlayer = UTIL_PlayerByIndex( i );
		CCSPlayer* pPlayer = ToCSPlayer( basePlayer );
		
		if( pPlayer && pPlayer->AttemptedToDefuseBomb() )
			data1++;
	}

	if( data1 > 1 )
		return true; // we need at least two of these guys to be interesting

	return false;

}

bool FFEVAL_KNIFE_LEVEL_REACHED( int &iPlayer, int &data1, int &data2, int &data3 )
{
	CBasePlayer *basePlayer = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		basePlayer = UTIL_PlayerByIndex( i );
		CCSPlayer* pPlayer = ToCSPlayer( basePlayer );
		CWeaponCSBase* attackerWeapon = pPlayer ? pPlayer->GetActiveCSWeapon() : 0;
		CSWeaponID attackerWeaponID = attackerWeapon ? attackerWeapon->GetCSWeaponID() : WEAPON_NONE; 
		if( attackerWeaponID == WEAPON_KNIFE )
			data1++;
	}

	if( data1 > 3 )
		return true; // we need at least three of these guys to be interesting

	return false;
}
		

bool FFEVAL_WON_AS_LAST_MEMBER( int &iPlayer, int &data1, int &data2, int &data3 )
{
	CCSPlayer *pCSPlayer = NULL;
	int winningTeam = CSGameRules()->m_iRoundWinStatus;

	if (winningTeam != TEAM_TERRORIST && winningTeam != TEAM_CT)
	{
		return false;
	}

	int losingTeam = (winningTeam == TEAM_TERRORIST) ? TEAM_CT : TEAM_TERRORIST;

	CCSGameRules::TeamPlayerCounts playerCounts[TEAM_MAXCOUNT];
	CSGameRules()->GetPlayerCounts(playerCounts);

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pCSPlayer = ToCSPlayer(UTIL_PlayerByIndex( i ) );
		if( pCSPlayer && pCSPlayer->GetTeamNumber() == winningTeam && pCSPlayer->IsAlive())
		{
			//Check if the player is still the only living member of his team ( on the off chance that a player joins late)
			//This check is a little hacky. We make sure that there are no enemies alive. Since the bomb causes the round to end before exploding,
			//the only way for only 1 person to be alive at round win time is extermination or defuse (in both cases, the last living player caused the win)
			if (playerCounts[winningTeam].totalAlivePlayers == 1 && playerCounts[losingTeam].totalAlivePlayers == 0)
			{
				const PlayerStats_t& playerStats = CCS_GameStats.FindPlayerStats( pCSPlayer );
				iPlayer = i;
				data1 = playerStats.statsCurrentRound[CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE];
				if (data1 >= 2)
				{
					return true;
				}
			}
		}
	}

	return false;
}

int FFEVAL_PULLED_TRIGGER( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumTriggerPulls();
}
int FFEVAL_SURVIVAL_TIME( CCSPlayer* pPlayer )
{
	return (int) pPlayer->GetLongestSurvivalTime();
}
int FFEVAL_MOLOTOV_BURNS( CCSPlayer* pPlayer )
{
	// DDK: note that this gets all burn damage.  Currently only molotov's have burn damage
	return pPlayer->GetNumPlayersDamagedWithFire();
}
int FFEVAL_KNIFE_WITHOUT_AMMO( CCSPlayer* pPlayer )
{
	return pPlayer->GetKnifeKillsWhenOutOfAmmo();
}
int FFEVAL_KNIFE_BOMB_PLANTER( CCSPlayer* pPlayer )
{
	return pPlayer->GetKnifeLevelKilledBombPlacer();
}

int FFEVAL_BOMB_CARRIERS( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumBombCarrierKills();
}
int FFEVAL_KNIFE_IN_GUNFIGHT( CCSPlayer* pPlayer )
{
	return pPlayer->WasWieldingKnifeAndKilledByGun() ? 1 : 0;
}

int FFEVAL_MULTIPLE_ATTACKER_COUNT( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumEnemyDamagers();
}

int FFEVAL_USED_ALL_AMMO( CCSPlayer* pPlayer )
{
    CWeaponCSBase *pRifleWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE ));
    CWeaponCSBase *pHandgunWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL ));
    if ( pRifleWeapon && !pRifleWeapon->HasAmmo() && pHandgunWeapon && !pHandgunWeapon->HasAmmo() )
		return 1;
	else
		return 0;
}

int FFEVAL_DAMAGE_MULTIPLE_ENEMIES( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumEnemiesDamaged();
}

int FFEVAL_USED_MULTIPLE_WEAPONS( CCSPlayer* pPlayer )
{
	return pPlayer->GetNumFirearmsUsed();
}

int FFEVAL_DEFUSED_WITH_DROPPED_KIT( CCSPlayer* pPlayer )
{
    return pPlayer->GetDefusedWithPickedUpKit() ? 1 : 0;
}

bool funfact_helper_defuse_close_call( int &iPlayer, int &data1, int &data2, int &data3, float flThreshold, float flFractionalMultiplier )
{
	CBasePlayer *pPlayer = NULL;
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        pPlayer = UTIL_PlayerByIndex( i );

		// Only CTs can defuse
        if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
		{
			CCSPlayer *pCSPlayer = (CCSPlayer*)pPlayer;
			if ( pCSPlayer )
			{
				float flDefuseTimeRemaining = pCSPlayer->GetDefusedBombWithThisTimeRemaining();
				if ( flDefuseTimeRemaining > 0 && flDefuseTimeRemaining <= flThreshold )
				{
					iPlayer = i;
					data1 = RoundFloatToInt( flDefuseTimeRemaining * flFractionalMultiplier );
					return true;
				}
			}
		}
	}

	return false;
}

bool FFEVAL_DEFUSE_WAS_CLOSE_CALL_TENTHS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return funfact_helper_defuse_close_call( iPlayer, data1, data2, data3, 0.5f, 10.0f );	
}

bool FFEVAL_DEFUSE_WAS_CLOSE_CALL_HUNDREDTHS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return funfact_helper_defuse_close_call( iPlayer, data1, data2, data3, 0.1f, 100.0f );
}

bool FFEVAL_DEFUSE_WAS_CLOSE_CALL_THOUSANDTHS( int &iPlayer, int &data1, int &data2, int &data3 )
{
	return funfact_helper_defuse_close_call( iPlayer, data1, data2, data3, 0.01f, 1000.0f );
}

bool FFEVAL_T_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float terroristAccuracy = GetTeamAccuracy(TEAM_TERRORIST);
	float ctAccuracy = GetTeamAccuracy(TEAM_CT);

	if (terroristAccuracy > kMinTeamAccuracy && terroristAccuracy > ctAccuracy)
	{
		data1 = RoundFloatToInt(terroristAccuracy * 100.0f);
		return true;
	}
	return false;
}

bool FFEVAL_CT_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float terroristAccuracy = GetTeamAccuracy(TEAM_TERRORIST);
	float ctAccuracy = GetTeamAccuracy(TEAM_CT);

	if (ctAccuracy > kMinTeamAccuracy && ctAccuracy > terroristAccuracy)
	{
		data1 = RoundFloatToInt(ctAccuracy * 100.0f);
		return true;
	}
	return false;
}

bool FFEVAL_BEST_TERRORIST_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
    float fAccuracy = 0.0f, fBestAccuracy = 0.0f;
    CBasePlayer *pPlayer = NULL;
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        pPlayer = UTIL_PlayerByIndex( i );

        // Look only at terrorist players
        if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_TERRORIST )
        {
            // Calculate accuracy the terrorist
            float shots = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
            float hits = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
            if (shots > kMinShotsForAccuracy)
            {
                fAccuracy = (float)hits / shots;
            }

            // Track the most accurate terrorist
            if ( fAccuracy > fBestAccuracy )
            {
                fBestAccuracy = fAccuracy;
                iPlayer = i;
            }
        }
    }

    if ( fBestAccuracy - GetTeamAccuracy( TEAM_TERRORIST ) >= kAccuracyAdvantage )
    {
		data1 = RoundFloatToInt(fBestAccuracy * 100.0f);
        data2 = RoundFloatToInt(GetTeamAccuracy( TEAM_TERRORIST ) * 100.0f);
        return true;
    }

    return false;
}

bool FFEVAL_BEST_COUNTERTERRORIST_ACCURACY( int &iPlayer, int &data1, int &data2, int &data3 )
{
	float fAccuracy = 0.0f, fBestAccuracy = 0.0f;
    CBasePlayer *pPlayer = NULL;
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        pPlayer = UTIL_PlayerByIndex( i );

        // Look only at counter-terrorist players
        if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
        {
            // Calculate accuracy the counter-terrorist
            float shots = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_FIRED];
            float hits = CCS_GameStats.FindPlayerStats(pPlayer).statsCurrentRound[CSSTAT_SHOTS_HIT];
            if (shots > kMinShotsForAccuracy)
            {
                fAccuracy = (float)hits / shots;
            }

            // Track the most accurate counter-terrorist
            if ( fAccuracy > fBestAccuracy )
            {
                fBestAccuracy = fAccuracy;
                iPlayer = i;
            }
        }
    }

	if ( fBestAccuracy - GetTeamAccuracy( TEAM_CT ) >= kAccuracyAdvantage )
	{
		data1 = RoundFloatToInt(fBestAccuracy * 100.0f);
		data2 = RoundFloatToInt(GetTeamAccuracy( TEAM_CT ) * 100.0f);
        return true;
    }

    return false;
}


//=============================================================================
// Fun Fact Declarations
//=============================================================================

DECLARE_FUNFACT_STATBEST(	FF_KILL_WOUNDED_ENEMIES,	"#funfact_kill_wounded_enemies",			0.2f,	CSSTAT_KILLS_ENEMY_WOUNDED,			3,		GameFlags::AllModes | GameFlags::Elimination,	PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_KILLS_HEADSHOTS,			"#funfact_kills_headshots",					0.4f,	CSSTAT_KILLS_HEADSHOT,				2,		GameFlags::AllModes,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_KILLS_WITH_STATTRAK_WEAPON,	"#funfact_kills_with_stattrak_weapon",	0.3f,	CSSTAT_KILLS_WITH_STATTRAK_WEAPON,	3,		GameFlags::AllModes,	                        PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_NUM_TIMES_JUMPED,		"#funfact_num_times_jumped",				0.1f,	CSSTAT_TOTAL_JUMPS,					20,		GameFlags::AllModes,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_KILLED_ENEMIES,			"#funfact_killed_enemies",					0.4f,	CSSTAT_KILLS,						3,		GameFlags::NotGunGame | GameFlags::Elimination,	PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_KNIFE_KILLS,				"#funfact_knife_kills",						0.4f,	CSSTAT_KILLS_KNIFE,					1,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_REVENGE,					"#funfact_revenge",							0.3f,	CSSTAT_REVENGES,					1,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_BLIND_KILLS,				"#funfact_blind_kills",						0.9f,	CSSTAT_KILLS_WHILE_BLINDED,			1,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_DAMAGE_WITH_GRENADES,	"#funfact_damage_with_grenade",				0.5f,	CSSTAT_GRENADE_DAMAGE,				150,	GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_KILLS_WITH_GRENADES,		"#funfact_kills_grenades",					0.7f, 	CSSTAT_KILLS_HEGRENADE,				2,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_POSTHUMOUS_GRENADE_KILLS,"#funfact_posthumous_kills_with_grenade",	1.0f,	CSSTAT_GRENADE_POSTHUMOUSKILLS,		1,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_GRENADES_THROWN,			"#funfact_grenades_thrown",					0.3f,	CSSTAT_GRENADES_THROWN,				3,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_DEFENDED_BOMB,			"#funfact_defended_bomb",					0.5f,	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,	2,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_KILLS_WITH_LAST_ROUND,	"#funfact_kills_with_last_round",			0.6f,	CSSTAT_KILLS_WITH_LAST_ROUND,		1,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_DONATED_WEAPONS,			"#funfact_donated_weapons",					0.3f,	CSSTAT_WEAPONS_DONATED,				2,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_ITEMS_PURCHASED,			"#funfact_items_purchased",					0.2f,	CSSTAT_ITEMS_PURCHASED,				4,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_FALL_DAMAGE,				"#funfact_fall_damage",						0.2f,	CSSTAT_FALL_DAMAGE,					50,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_BROKE_WINDOWS,			"#funfact_broke_windows",					0.1f,	CSSTAT_NUM_BROKEN_WINDOWS,			5,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_TASER_KILL,				"#funfact_taser_kill",						0.7f,	CSSTAT_KILLS_TASER,					1,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_NUMBER_OF_OVERKILLS,		"#funfact_number_of_overkills",				0.5f,	CSSTAT_DOMINATION_OVERKILLS,		2,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_MONEY_SPENT,				"#funfact_money_spent",						0.2f,	CSSTAT_MONEY_SPENT,					5000,	GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_STATBEST(	FF_ITEMS_DROPPED_VALUE,		"#funfact_items_dropped_value",				0.4f,	CSTAT_ITEMS_DROPPED_VALUE,			10000,	GameFlags::Classic,								PlayerFlags::HighestOnly );

DECLARE_FUNFACT_STATSUM(	FF_SHOTS_FIRED,				"#funfact_shots_fired",						0.2f,	CSSTAT_SHOTS_FIRED,					100,	GameFlags::NotGunGame,							PlayerFlags::All );
DECLARE_FUNFACT_STATSUM(	FF_SHOTS_FIRED_GG,			"#funfact_shots_fired",						0.1f,	CSSTAT_SHOTS_FIRED,					250,	GameFlags::GunGame,								PlayerFlags::All );

DECLARE_FUNFACT_PLAYERFUNC( FF_BEST_ACCURACY,			"#funfact_best_accuracy",					0.2f,	FFEVAL_ACCURACY,					50,		GameFlags::AllModes,							PlayerFlags::HighestOnly | PlayerFlags::Alive );
DECLARE_FUNFACT_PLAYERFUNC(	FF_BOTS_ASSUMED,			"#funfact_bots_assumed",					0.2f, 	FFEVAL_MOST_BOTS_ASSUMED,			2,		GameFlags::AllModes,							PlayerFlags::HighestOnly | PlayerFlags::AllowIfControlledBot );
DECLARE_FUNFACT_PLAYERFUNC(	FF_DOMINATION,				"#funfact_domination",						0.3f, 	FFEVAL_MOST_CONCURRENT_DOMINATIONS,	1,		GameFlags::AllModes,							PlayerFlags::HighestOnly | PlayerFlags::HasKilledThisRound );
DECLARE_FUNFACT_PLAYERFUNC(	FF_USED_ALL_AMMO,			"#funfact_used_all_ammo",					0.5f,	FFEVAL_USED_ALL_AMMO,				1,		GameFlags::NotGunGame,							PlayerFlags::All );
DECLARE_FUNFACT_PLAYERFUNC(	FF_DAMAGE_NO_KILLS,			"#funfact_damage_no_kills",					0.2f, 	FFEVAL_DAMAGE_NO_KILLS,				250,	GameFlags::NotGunGame,							PlayerFlags::All );
DECLARE_FUNFACT_PLAYERFUNC(	FF_SINGLE_GRENADE_KILLS,	"#funfact_kills_with_single_grenade",		0.8f, 	FFEVAL_KILLS_SINGLE_GRENADE,		2,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_EMPTY_GUNS,				"#funfact_empty_guns",						0.3f, 	FFEVAL_EMPTY_GUNS,					2,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_MULTIPLE_ATTACKS_LIVED,	"#funfact_survived_multiple_attackers",		0.4f,	FFEVAL_MULTIPLE_ATTACKER_COUNT,		3,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly | PlayerFlags::Alive );
DECLARE_FUNFACT_PLAYERFUNC(	FF_MULTIPLE_ATTACKS_DIED,	"#funfact_died_from_multiple_attackers",	0.3f,	FFEVAL_MULTIPLE_ATTACKER_COUNT,		3,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly | PlayerFlags::Dead );
DECLARE_FUNFACT_PLAYERFUNC(	FF_DAMAGE_MULTIPLE_ENEMIES,	"#funfact_damage_multiple_enemies",			0.2f,	FFEVAL_DAMAGE_MULTIPLE_ENEMIES,		3,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_BOMB_CARRIERS,			"#funfact_bomb_carriers",					0.3f, 	FFEVAL_BOMB_CARRIERS,				2,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_KNIFE_BOMB_PLANTER,		"#funfact_knife_bomb_planter",				0.8f, 	FFEVAL_KNIFE_BOMB_PLANTER,			1,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly | PlayerFlags::Alive );
DECLARE_FUNFACT_PLAYERFUNC(	FF_KNIFE_WITHOUT_AMMO,		"#funfact_knife_without_ammo",				0.3f, 	FFEVAL_KNIFE_WITHOUT_AMMO,			2,		GameFlags::NotGunGame,							PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_KILL_DEFUSER,			"#funfact_kill_defuser",					0.5f, 	FFEVAL_KILLED_DEFUSER,				1,		GameFlags::NotGunGame,							PlayerFlags::TeamTerrorist | PlayerFlags::WinningTeam );
DECLARE_FUNFACT_PLAYERFUNC( FF_KILL_RESCUER,			"#funfact_kill_rescuer",					0.5f, 	FFEVAL_KILLED_RESCUER,				1,		GameFlags::Classic,								PlayerFlags::TeamTerrorist );
DECLARE_FUNFACT_PLAYERFUNC(	FF_DEFAULT_WEAPON,			"#funfact_default_weapon",					0.1f, 	FFEVAL_DEFAULT_WEAPON,				1,		GameFlags::Classic,								PlayerFlags::WinningTeam | PlayerFlags::HasKilledThisRound | PlayerFlags::Alive );
DECLARE_FUNFACT_PLAYERFUNC(	FF_ROUNDS_WITHOUT_DYING,	"#funfact_rounds_without_dying",			0.6f, 	FFEVAL_ROUNDS_WITHOUT_DYING,		3,		GameFlags::Classic,								PlayerFlags::HighestOnly | PlayerFlags::WinningTeam | PlayerFlags::Alive );
DECLARE_FUNFACT_PLAYERFUNC( FF_KNIFE_IN_GUNFIGHT,		"#funfact_knife_in_gunfight",				0.4f, 	FFEVAL_KNIFE_IN_GUNFIGHT ,			1,		GameFlags::Classic,								PlayerFlags::All );
DECLARE_FUNFACT_PLAYERFUNC(	FF_USED_MULTIPLE_WEAPONS,	"#funfact_used_multiple_weapons",			0.5f,	FFEVAL_USED_MULTIPLE_WEAPONS,		4,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_DEFUSED_WITH_DROPPED_KIT,"#funfact_defused_with_dropped_kit",        0.5f, 	FFEVAL_DEFUSED_WITH_DROPPED_KIT,	1,		GameFlags::Classic,								PlayerFlags::TeamCT );
DECLARE_FUNFACT_PLAYERFUNC(	FF_KILLED_HALF_OF_ENEMIES,  "#funfact_killed_half_of_enemies",          0.4f, 	FFEVAL_KILLED_PERCENT_OF_ENEMIES,	50,		GameFlags::NotGunGame,							PlayerFlags::WinningTeam | PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_KILLED_ALL_ENEMIES,      "#funfact_ace",                             0.9f, 	FFEVAL_KILLED_PERCENT_OF_ENEMIES,	100,	GameFlags::Classic,							    PlayerFlags::WinningTeam | PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_MOLOTOV_BURNS,			"#funfact_molotov_burns",					0.3f, 	FFEVAL_MOLOTOV_BURNS,				2,		GameFlags::Classic,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_MAX_KILLED_BEFORE_DYING,	"#funfact_killed_before_dying",				0.3f, 	FFEVAL_KILL_STREAK_BEFORE_DYING,	3,		GameFlags::GunGame,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_MAX_RESPAWNS,			"#funfact_respawned",						0.3f, 	FFEVAL_MAX_NUM_RESPAWNS,			3,		GameFlags::GunGame,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_SURVIVAL_TIME,			"#funfact_survival_time",					0.2f, 	FFEVAL_SURVIVAL_TIME,				20,		GameFlags::GunGame,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_PULLED_TRIGGER,			"#funfact_pulled_trigger",					0.1f, 	FFEVAL_PULLED_TRIGGER,				20,		GameFlags::GunGame,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_STEPS_TAKEN,				"#funfact_steps_taken",						0.1f, 	FFEVAL_STEPS_TAKEN,					100,	GameFlags::GunGame,								PlayerFlags::HighestOnly );
DECLARE_FUNFACT_PLAYERFUNC(	FF_QUARTER_HEALTH,			"#funfact_quarter_health",					0.3f, 	FFEVAL_QUARTER_HEALTH,				2,		GameFlags::GunGame,								PlayerFlags::HighestOnly );

DECLARE_FUNFACT_EVALFUNC(	FF_DEFUSE_WAS_CLOSE_CALL_TENTHS,		"#funfact_defuse_was_close_call_tenths",		0.8f,	FFEVAL_DEFUSE_WAS_CLOSE_CALL_TENTHS,		GameFlags::Classic );
DECLARE_FUNFACT_EVALFUNC(	FF_DEFUSE_WAS_CLOSE_CALL_HUNDREDTHS,	"#funfact_defuse_was_close_call_hundredths",	0.9f,	FFEVAL_DEFUSE_WAS_CLOSE_CALL_HUNDREDTHS,	GameFlags::Classic );
DECLARE_FUNFACT_EVALFUNC(	FF_DEFUSE_WAS_CLOSE_CALL_THOUSANDTHS,	"#funfact_defuse_was_close_call_thousandths",	1.0f,	FFEVAL_DEFUSE_WAS_CLOSE_CALL_THOUSANDTHS,	GameFlags::Classic );

DECLARE_FUNFACT_EVALFUNC(	FF_CT_WIN_NO_KILLS,			"#funfact_ct_win_no_kills",					0.4f,	FFEVAL_CT_WIN_NO_KILLS,						GameFlags::NotGunGame ); 
DECLARE_FUNFACT_EVALFUNC( 	FF_T_WIN_NO_KILLS,			"#funfact_t_win_no_kills",					0.4f, 	FFEVAL_T_WIN_NO_KILLS,						GameFlags::NotGunGame ); 
DECLARE_FUNFACT_EVALFUNC( 	FF_T_WIN_NO_CASUALTIES,		"#funfact_t_win_no_casualties",				0.2f, 	FFEVAL_T_WIN_NO_CASUALTIES,					GameFlags::NotGunGame ); 
DECLARE_FUNFACT_EVALFUNC( 	FF_CT_WIN_NO_CASUALTIES,	"#funfact_ct_win_no_casualties",			0.2f, 	FFEVAL_CT_WIN_NO_CASUALTIES,				GameFlags::NotGunGame );
DECLARE_FUNFACT_EVALFUNC(	FF_FIRST_KILL,				"#funfact_first_kill",						0.2f, 	FFEVAL_FIRST_KILL,							GameFlags::NotGunGame );
DECLARE_FUNFACT_EVALFUNC(	FF_FIRST_BLOOD,				"#funfact_first_blood",						0.2f, 	FFEVAL_FIRST_BLOOD,							GameFlags::NotGunGame );
DECLARE_FUNFACT_EVALFUNC( 	FF_WON_AS_LAST_MEMBER,		"#funfact_won_as_last_member",				0.6f, 	FFEVAL_WON_AS_LAST_MEMBER,					GameFlags::NotGunGame );
DECLARE_FUNFACT_EVALFUNC( 	FF_SHORT_ROUND,				"#funfact_short_round",						0.2f, 	FFEVAL_SHORT_ROUND,							GameFlags::Classic );
DECLARE_FUNFACT_EVALFUNC(	FF_T_ACCURACY,				"#funfact_terrorist_accuracy",				0.1f,	FFEVAL_T_ACCURACY,							GameFlags::AllModes );
DECLARE_FUNFACT_EVALFUNC(	FF_CT_ACCURACY,				"#funfact_ct_accuracy",						0.1f,	FFEVAL_CT_ACCURACY,							GameFlags::AllModes );
DECLARE_FUNFACT_EVALFUNC(	FF_BEST_T_ACCURACY,			"#funfact_best_terrorist_accuracy",			0.1f,	FFEVAL_BEST_TERRORIST_ACCURACY,				GameFlags::AllModes );
DECLARE_FUNFACT_EVALFUNC(	FF_BEST_CT_ACCURACY,		"#funfact_best_counterterrorist_accuracy",	0.1f,	FFEVAL_BEST_COUNTERTERRORIST_ACCURACY,		GameFlags::AllModes );
DECLARE_FUNFACT_EVALFUNC(	FF_KNIFE_LEVEL_REACHED,     "#funfact_knife_level_reached",				0.3f,	FFEVAL_KNIFE_LEVEL_REACHED,					GameFlags::GunGame );
DECLARE_FUNFACT_EVALFUNC(	FF_TICKING_TIME,			"#funfact_ticking_time",					0.3f, 	FFEVAL_TICKING_TIME,						GameFlags::Demolition );
DECLARE_FUNFACT_EVALFUNC( 	FF_CT_WIN_TIME,				"#funfact_ct_win_time",						0.2f,	FFEVAL_CT_WIN_TIME ,						GameFlags::NotGunGame | GameFlags::Elimination );
DECLARE_FUNFACT_EVALFUNC( 	FF_TER_WIN_TIME,			"#funfact_ter_win_time",					0.2f,	FFEVAL_TER_WIN_TIME,						GameFlags::NotGunGame | GameFlags::Elimination );
DECLARE_FUNFACT_EVALFUNC( 	FF_PICKUP_BOMB,				"#funfact_pickup_bomb",						0.3f,	FFEVAL_PICKUP_BOMB,							GameFlags::Demolition );
DECLARE_FUNFACT_EVALFUNC(	FF_BOMB_PLANTED_BEFORE_KILL,"#funfact_bomb_planted_before_kill",		0.3f, 	FFEVAL_BOMB_PLANTED_BEFORE_KILL,			GameFlags::Demolition );
DECLARE_FUNFACT_EVALFUNC(	FF_FAILED_BOMB_PLANTS,		"#funfact_failed_bomb_plants",				0.3f, 	FFEVAL_FAILED_BOMB_PLANTS,					GameFlags::Demolition );

DECLARE_FUNFACT_EVALFUNC(	FF_FALLBACK,				"",											0.0f, 	FFEVAL_ALWAYS_TRUE,							GameFlags::AllModes );
