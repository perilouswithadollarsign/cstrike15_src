//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================
#ifndef CS_GAMESTATS_SHARED_H
#define CS_GAMESTATS_SHARED_H
#ifdef _WIN32
#pragma once
#endif
#include "cbase.h"
// #include "tier1/utlvector.h"
// #include "tier1/utldict.h"
#include "shareddefs.h"
#include "cs_shareddefs.h"
#include "cs_weapon_parse.h"


#define CS_NUM_LEVELS 18

//=============================================================================
// Helper class for simple manipulation of bit arrays.
// Used for server->client packets containing delta stats
//=============================================================================

template <int BitLength>
class BitArray
{
	enum { ByteLength = (BitLength + 7) / 8 };
public:
	BitArray() { ClearAll(); }

	void SetBit(int n) { m_bytes[n / 8] |= 1 << (n & 7); }
	void ClearBit(int n) { m_bytes[n / 8] &= (~(1 << (n & 7))); }
	bool IsBitSet(int n) const { return (m_bytes[n / 8] & (1 << (n & 7))) != 0;}

	void ClearAll() { V_memset(m_bytes, 0, sizeof(m_bytes)); }
	int NumBits() { return BitLength; }
	int NumBytes() { return ByteLength; }

	byte* RawPointer() { return m_bytes; }

private:
	byte m_bytes[ByteLength];
};


//=============================================================================
//
// CS Game Stats Enums
//
// approprate location.

enum CSStatType_t
{
	CSSTAT_UNDEFINED = -1,
	CSSTAT_SHOTS_HIT,
	CSSTAT_SHOTS_FIRED,
	CSSTAT_KILLS,
	CSSTAT_DEATHS,
	CSSTAT_DAMAGE,
	CSSTAT_NUM_BOMBS_PLANTED,
	CSSTAT_NUM_BOMBS_DEFUSED,
	CSSTAT_TR_NUM_BOMBS_PLANTED,
	CSSTAT_TR_NUM_BOMBS_DEFUSED,
	CSSTAT_PLAYTIME,
	CSSTAT_ROUNDS_WON,
	CSSTAT_T_ROUNDS_WON,
	CSSTAT_CT_ROUNDS_WON,
	CSSTAT_ROUNDS_PLAYED,
	CSSTAT_PISTOLROUNDS_WON,
	CSTAT_GUNGAME_ROUNDS_WON,
	CSTAT_GUNGAME_ROUNDS_PLAYED,
	CSSTAT_MONEY_EARNED,
	CSSTAT_OBJECTIVES_COMPLETED,
	CSSTAT_BOMBS_DEFUSED_WITHKIT,

	CSSTAT_KILLS_DEAGLE,
	CSSTAT_KILLS_USP,
	CSSTAT_KILLS_GLOCK,
	CSSTAT_KILLS_P228,
	CSSTAT_KILLS_ELITE,
	CSSTAT_KILLS_FIVESEVEN,
	CSSTAT_KILLS_AWP,
	CSSTAT_KILLS_AK47,
	CSSTAT_KILLS_M4A1,
	CSSTAT_KILLS_AUG,
	CSSTAT_KILLS_SG552,
	CSSTAT_KILLS_SG550,
	CSSTAT_KILLS_GALIL,
	CSSTAT_KILLS_GALILAR,
	CSSTAT_KILLS_FAMAS,
	CSSTAT_KILLS_SCOUT,
	CSSTAT_KILLS_G3SG1,
	CSSTAT_KILLS_P90,
	CSSTAT_KILLS_MP5NAVY,
	CSSTAT_KILLS_TMP,
	CSSTAT_KILLS_MAC10,
	CSSTAT_KILLS_UMP45,
	CSSTAT_KILLS_M3,
	CSSTAT_KILLS_XM1014,
	CSSTAT_KILLS_M249,
	CSSTAT_KILLS_KNIFE,
	CSSTAT_KILLS_HEGRENADE,
	CSSTAT_KILLS_MOLOTOV,
	CSSTAT_KILLS_DECOY,
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
	CSSTAT_KILLS_SCAR17,
	CSSTAT_KILLS_SCAR20,
	CSSTAT_KILLS_SG556,
	CSSTAT_KILLS_SSG08,

	CSSTAT_SHOTS_DEAGLE,
	CSSTAT_SHOTS_USP,
	CSSTAT_SHOTS_GLOCK,
	CSSTAT_SHOTS_P228,
	CSSTAT_SHOTS_ELITE,
	CSSTAT_SHOTS_FIVESEVEN,
	CSSTAT_SHOTS_AWP,
	CSSTAT_SHOTS_AK47,
	CSSTAT_SHOTS_M4A1,
	CSSTAT_SHOTS_AUG,
	CSSTAT_SHOTS_SG552,
	CSSTAT_SHOTS_SG550,
	CSSTAT_SHOTS_GALIL,
	CSSTAT_SHOTS_GALILAR,
	CSSTAT_SHOTS_FAMAS,
	CSSTAT_SHOTS_SCOUT,
	CSSTAT_SHOTS_G3SG1,
	CSSTAT_SHOTS_P90,
	CSSTAT_SHOTS_MP5NAVY,
	CSSTAT_SHOTS_TMP,
	CSSTAT_SHOTS_MAC10,
	CSSTAT_SHOTS_UMP45,
	CSSTAT_SHOTS_M3,
	CSSTAT_SHOTS_XM1014,
	CSSTAT_SHOTS_M249,
	CSSTAT_SHOTS_BIZON,
	CSSTAT_SHOTS_MAG7,
	CSSTAT_SHOTS_NEGEV,
	CSSTAT_SHOTS_SAWEDOFF,
	CSSTAT_SHOTS_TEC9,
	CSSTAT_SHOTS_TASER,
	CSSTAT_SHOTS_HKP2000,
	CSSTAT_SHOTS_MP7,
	CSSTAT_SHOTS_MP9,
	CSSTAT_SHOTS_NOVA,
	CSSTAT_SHOTS_P250,
	CSSTAT_SHOTS_SCAR17,
	CSSTAT_SHOTS_SCAR20,
	CSSTAT_SHOTS_SG556,
	CSSTAT_SHOTS_SSG08,

	CSSTAT_HITS_DEAGLE,
	CSSTAT_HITS_USP,
	CSSTAT_HITS_GLOCK,
	CSSTAT_HITS_P228,
	CSSTAT_HITS_ELITE,
	CSSTAT_HITS_FIVESEVEN,
	CSSTAT_HITS_AWP,
	CSSTAT_HITS_AK47,
	CSSTAT_HITS_M4A1,
	CSSTAT_HITS_AUG,
	CSSTAT_HITS_SG552,
	CSSTAT_HITS_SG550,
	CSSTAT_HITS_GALIL,
	CSSTAT_HITS_GALILAR,
	CSSTAT_HITS_FAMAS,
	CSSTAT_HITS_SCOUT,
	CSSTAT_HITS_G3SG1,
	CSSTAT_HITS_P90,
	CSSTAT_HITS_MP5NAVY,
	CSSTAT_HITS_TMP,
	CSSTAT_HITS_MAC10,
	CSSTAT_HITS_UMP45,
	CSSTAT_HITS_M3,
	CSSTAT_HITS_XM1014,
	CSSTAT_HITS_M249,
	CSSTAT_HITS_BIZON,
	CSSTAT_HITS_MAG7,
	CSSTAT_HITS_NEGEV,
	CSSTAT_HITS_SAWEDOFF,
	CSSTAT_HITS_TEC9,
	CSSTAT_HITS_TASER,
	CSSTAT_HITS_HKP2000,
	CSSTAT_HITS_MP7,
	CSSTAT_HITS_MP9,
	CSSTAT_HITS_NOVA,
	CSSTAT_HITS_P250,
	CSSTAT_HITS_SCAR17,
	CSSTAT_HITS_SCAR20,
	CSSTAT_HITS_SG556,
	CSSTAT_HITS_SSG08,

	CSSTAT_DAMAGE_DEAGLE,
	CSSTAT_DAMAGE_USP,
	CSSTAT_DAMAGE_GLOCK,
	CSSTAT_DAMAGE_P228,
	CSSTAT_DAMAGE_ELITE,
	CSSTAT_DAMAGE_FIVESEVEN,
	CSSTAT_DAMAGE_AWP,
	CSSTAT_DAMAGE_AK47,
	CSSTAT_DAMAGE_M4A1,
	CSSTAT_DAMAGE_AUG,
	CSSTAT_DAMAGE_SG552,
	CSSTAT_DAMAGE_SG550,
	CSSTAT_DAMAGE_GALIL,
	CSSTAT_DAMAGE_GALILAR,
	CSSTAT_DAMAGE_FAMAS,
	CSSTAT_DAMAGE_SCOUT,
	CSSTAT_DAMAGE_G3SG1,
	CSSTAT_DAMAGE_P90,
	CSSTAT_DAMAGE_MP5NAVY,
	CSSTAT_DAMAGE_TMP,
	CSSTAT_DAMAGE_MAC10,
	CSSTAT_DAMAGE_UMP45,
	CSSTAT_DAMAGE_M3,
	CSSTAT_DAMAGE_XM1014,
	CSSTAT_DAMAGE_M249,
	CSSTAT_DAMAGE_BIZON,
	CSSTAT_DAMAGE_MAG7,
	CSSTAT_DAMAGE_NEGEV,
	CSSTAT_DAMAGE_SAWEDOFF,
	CSSTAT_DAMAGE_TEC9,
	CSSTAT_DAMAGE_TASER,
	CSSTAT_DAMAGE_HKP2000,
	CSSTAT_DAMAGE_MP7,
	CSSTAT_DAMAGE_MP9,
	CSSTAT_DAMAGE_NOVA,
	CSSTAT_DAMAGE_P250,
	CSSTAT_DAMAGE_SCAR17,
	CSSTAT_DAMAGE_SCAR20,
	CSSTAT_DAMAGE_SG556,
	CSSTAT_DAMAGE_SSG08,

	CSSTAT_KILLS_HEADSHOT,
	CSSTAT_KILLS_ENEMY_BLINDED,
	CSSTAT_KILLS_WHILE_BLINDED,
	CSSTAT_KILLS_WITH_LAST_ROUND,
	CSSTAT_KILLS_ENEMY_WEAPON,
	CSSTAT_KILLS_KNIFE_FIGHT,
	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,
	CSSTAT_KILLS_WITH_STATTRAK_WEAPON,

	CSSTAT_DECAL_SPRAYS,
	CSSTAT_TOTAL_JUMPS,
	CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE,
	CSSTAT_KILLS_ENEMY_WOUNDED,
	CSSTAT_FALL_DAMAGE,

	CSSTAT_NUM_HOSTAGES_RESCUED,

	CSSTAT_NUM_BROKEN_WINDOWS,
	CSSTAT_PROPSBROKEN_ALL,
	CSSTAT_PROPSBROKEN_MELON,
	CSSTAT_PROPSBROKEN_OFFICEELECTRONICS,
	CSSTAT_PROPSBROKEN_OFFICERADIO,
	CSSTAT_PROPSBROKEN_OFFICEJUNK,
	CSSTAT_PROPSBROKEN_ITALY_MELON,

	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,

	CSSTAT_WEAPONS_DONATED,

	CSSTAT_ITEMS_PURCHASED,
	CSSTAT_MONEY_SPENT,

	CSSTAT_DOMINATIONS,
	CSSTAT_DOMINATION_OVERKILLS,
	CSSTAT_REVENGES,

	CSSTAT_MVPS,
	CSSTAT_CONTRIBUTION_SCORE,
	CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE,

	CSSTAT_GRENADE_DAMAGE,
	CSSTAT_GRENADE_POSTHUMOUSKILLS,
	CSSTAT_GRENADES_THROWN,

	CSTAT_ITEMS_DROPPED_VALUE,

	//Map win stats
	CSSTAT_MAP_WINS_CS_MILITIA,
	CSSTAT_MAP_WINS_CS_ASSAULT,
	CSSTAT_MAP_WINS_CS_ITALY,
	CSSTAT_MAP_WINS_CS_OFFICE,
	CSSTAT_MAP_WINS_DE_AZTEC,
	CSSTAT_MAP_WINS_DE_CBBLE,  
	CSSTAT_MAP_WINS_DE_DUST2,
	CSSTAT_MAP_WINS_DE_DUST,
	CSSTAT_MAP_WINS_DE_INFERNO,
	CSSTAT_MAP_WINS_DE_NUKE,
	CSSTAT_MAP_WINS_DE_PIRANESI,  
	CSSTAT_MAP_WINS_DE_PRODIGY,  
	CSSTAT_MAP_WINS_DE_LAKE,
	CSSTAT_MAP_WINS_DE_SAFEHOUSE,
	CSSTAT_MAP_WINS_DE_SHORTTRAIN,
	CSSTAT_MAP_WINS_DE_TRAIN,
	CSSTAT_MAP_WINS_DE_SUGARCANE,
	CSSTAT_MAP_WINS_DE_STMARC,
	CSSTAT_MAP_WINS_DE_BANK,
	CSSTAT_MAP_WINS_DE_EMBASSY,
	CSSTAT_MAP_WINS_DE_DEPOT,
	CSSTAT_MAP_WINS_DE_VERTIGO,
	CSSTAT_MAP_WINS_DE_BALKAN,
	CSSTAT_MAP_WINS_AR_MONASTERY,
	CSSTAT_MAP_WINS_AR_SHOOTS,
	CSSTAT_MAP_WINS_AR_BAGGAGE,


	CSSTAT_MAP_ROUNDS_CS_MILITIA,
	CSSTAT_MAP_ROUNDS_CS_ASSAULT,
	CSSTAT_MAP_ROUNDS_CS_ITALY,
	CSSTAT_MAP_ROUNDS_CS_OFFICE,
	CSSTAT_MAP_ROUNDS_DE_AZTEC,
	CSSTAT_MAP_ROUNDS_DE_CBBLE,
	CSSTAT_MAP_ROUNDS_DE_DUST2,
	CSSTAT_MAP_ROUNDS_DE_DUST,
	CSSTAT_MAP_ROUNDS_DE_INFERNO,
	CSSTAT_MAP_ROUNDS_DE_NUKE,
	CSSTAT_MAP_ROUNDS_DE_PIRANESI,
	CSSTAT_MAP_ROUNDS_DE_PRODIGY,
	CSSTAT_MAP_ROUNDS_DE_LAKE,
	CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE,
	CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN,
	CSSTAT_MAP_ROUNDS_DE_TRAIN,
	CSSTAT_MAP_ROUNDS_DE_SUGARCANE,
	CSSTAT_MAP_ROUNDS_DE_STMARC,
	CSSTAT_MAP_ROUNDS_DE_BANK,
	CSSTAT_MAP_ROUNDS_DE_EMBASSY,
	CSSTAT_MAP_ROUNDS_DE_DEPOT,
	CSSTAT_MAP_ROUNDS_DE_VERTIGO,
	CSSTAT_MAP_ROUNDS_DE_BALKAN,
	CSSTAT_MAP_ROUNDS_AR_MONASTERY,
	CSSTAT_MAP_ROUNDS_AR_SHOOTS,
	CSSTAT_MAP_ROUNDS_AR_BAGGAGE,

	CSSTAT_MAP_MATCHES_WON_SHOOTS,
	CSSTAT_MAP_MATCHES_WON_BAGGAGE,
	CSSTAT_MAP_MATCHES_WON_LAKE,
	CSSTAT_MAP_MATCHES_WON_SUGARCANE,
	CSSTAT_MAP_MATCHES_WON_STMARC,
	CSSTAT_MAP_MATCHES_WON_BANK,
	CSSTAT_MAP_MATCHES_WON_EMBASSY,
	CSSTAT_MAP_MATCHES_WON_DEPOT,
	CSSTAT_MAP_MATCHES_WON_SAFEHOUSE,
	CSSTAT_MAP_MATCHES_WON_SHORTTRAIN,
	CSSTAT_MAP_MATCHES_WON_TRAIN,

	CSSTAT_MATCHES_WON,
	CSSTAT_MATCHES_DRAW,
	CSSTAT_MATCHES_PLAYED,
	CSSTAT_GUN_GAME_MATCHES_WON,
	CSSTAT_GUN_GAME_MATCHES_PLAYED,
	CSSTAT_GUN_GAME_PROGRESSIVE_MATCHES_WON,
	CSSTAT_GUN_GAME_SELECT_MATCHES_WON,
	CSSTAT_GUN_GAME_TRBOMB_MATCHES_WON,

	CSSTAT_LASTMATCH_CONTRIBUTION_SCORE,
	CSSTAT_LASTMATCH_GG_PROGRESSIVE_CONTRIBUTION_SCORE,
	CSSTAT_LASTMATCH_T_ROUNDS_WON,
	CSSTAT_LASTMATCH_CT_ROUNDS_WON,
	CSSTAT_LASTMATCH_ROUNDS_WON,
	CSTAT_LASTMATCH_ROUNDS_PLAYED,
	CSSTAT_LASTMATCH_KILLS,
	CSSTAT_LASTMATCH_DEATHS,
	CSSTAT_LASTMATCH_MVPS,
	CSSTAT_LASTMATCH_DAMAGE,
	CSSTAT_LASTMATCH_MONEYSPENT,
	CSSTAT_LASTMATCH_DOMINATIONS,
	CSSTAT_LASTMATCH_REVENGES,
	CSSTAT_LASTMATCH_MAX_PLAYERS,
	CSSTAT_LASTMATCH_FAVWEAPON_ID,
	CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,
	CSSTAT_LASTMATCH_FAVWEAPON_HITS,
	CSSTAT_LASTMATCH_FAVWEAPON_KILLS,

	CSSTAT_MAX	//Must be last entry.
};


#define CSSTAT_FIRST (CSSTAT_UNDEFINED+1)
#define CSSTAT_LAST (CSSTAT_MAX-1)

//
// CS Game Stats Flags
//
#define CSSTAT_PRIORITY_MASK		0x000F
#define CSSTAT_PRIORITY_NEVER		0x0000		// not sent to client
#define CSSTAT_PRIORITY_ENDROUND	0x0001		// sent at end of round
#define CSSTAT_PRIORITY_LOW			0x0002		// sent every 2500ms
#define CSSTAT_PRIORITY_HIGH		0x0003		// sent every 250ms

struct CSStatProperty
{
	const char*	szSteamName;			// name of the stat on steam
	const char*	szLocalizationToken;	// localization token for the stat
	uint flags;							// priority flags for sending to client
};

extern CSStatProperty CSStatProperty_Table[];


//=============================================================================
//
// CS Player Round Stats
//
struct StatsCollection_t
{
	StatsCollection_t() { Reset(); }

	void Reset()
	{
		for ( int i = 0; i < ARRAYSIZE( m_iValue ); i++ )
		{
			m_iValue[i] = 0;
		}
	}

	int operator[] ( int index ) const
	{
		Assert(index >= 0 && index < ARRAYSIZE(m_iValue));
		return m_iValue[index];
	}

	int& operator[] ( int index )
	{
		Assert(index >= 0 && index < ARRAYSIZE(m_iValue));
		return m_iValue[index];
	}

	void Aggregate( const StatsCollection_t& other );

private:
	int m_iValue[CSSTAT_MAX];
};


struct RoundStatsDirectAverage_t
{
	float m_fStat[CSSTAT_MAX];


	RoundStatsDirectAverage_t()
	{
		Reset();
	}

	void Reset()
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] = 0;
		}
	}

	RoundStatsDirectAverage_t& operator +=( const StatsCollection_t &other )
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] += other[i];
		}
		return *this;
	}

	RoundStatsDirectAverage_t& operator /=( const float &divisor)
	{
		if (divisor > 0)
		{
			for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
			{
				m_fStat[i] /= divisor;
			}
		}
		return *this;
	}

	RoundStatsDirectAverage_t& operator *=( const float &divisor)
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] *= divisor;
		}
		return *this;
	}
};


struct RoundStatsRollingAverage_t
{
	float m_fStat[CSSTAT_MAX];
	int m_numberOfDataSets;

	RoundStatsRollingAverage_t()
	{
		Reset();
	}

	void Reset()
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] = 0;
		}
		m_numberOfDataSets = 0;
	}

	RoundStatsRollingAverage_t& operator +=( const RoundStatsRollingAverage_t &other )
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] += other.m_fStat[i];
		}
		return *this;
	}

    RoundStatsRollingAverage_t& operator +=( const StatsCollection_t &other )
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
            m_fStat[i] += other[i];
		}
		return *this;
	}

	RoundStatsRollingAverage_t& operator /=( const float &divisor)
	{
		if (divisor > 0)
		{
			for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
			{
				m_fStat[i] /= divisor;
			}
		}
		return *this;
	}

	void RollDataSetIntoAverage ( const RoundStatsRollingAverage_t &other )
	{
		for ( int i = 0; i < ARRAYSIZE( m_fStat ); i++ )
		{
			m_fStat[i] *= m_numberOfDataSets;
			m_fStat[i] += other.m_fStat[i];
			m_fStat[i] /= (m_numberOfDataSets + 1);
		}
		m_numberOfDataSets++;
	}
};

enum CSGameStatsVersions_t
{
	CS_GAMESTATS_FILE_VERSION = 006,
	CS_GAMESTATS_MAGIC = 0xDEADBEEF
};

struct CS_Gamestats_Version_t
{
	int m_iMagic;			// always CS_GAMESTATS_MAGIC
	int m_iVersion;
};


struct KillStats_t
{
	KillStats_t() { Reset(); }

	void Reset()
	{
		Q_memset( iNumKilled, 0, sizeof( iNumKilled ) );
		Q_memset( iNumKilledBy, 0, sizeof( iNumKilledBy ) );
		Q_memset( iNumKilledByUnanswered, 0, sizeof( iNumKilledByUnanswered ) );
	}

	int iNumKilled[MAX_PLAYERS+1];					// how many times this player has killed each other player
	int iNumKilledBy[MAX_PLAYERS+1];				// how many times this player has been killed by each other player
	int iNumKilledByUnanswered[MAX_PLAYERS+1];		// how many unanswered kills this player has been dealt by each other player
};

//=============================================================================
//
// CS Player Stats
//
struct PlayerStats_t
{
	PlayerStats_t()
	{
		Reset();
	}

	void Reset()
	{
		statsDelta.Reset();
		statsCurrentRound.Reset();
		statsCurrentMatch.Reset();
		statsKills.Reset();
	}

	PlayerStats_t( const PlayerStats_t &other )
	{
		statsDelta			= other.statsDelta;
		statsCurrentRound	= other.statsCurrentRound;
		statsCurrentMatch	= other.statsCurrentMatch;
	}

	StatsCollection_t	statsDelta;
	StatsCollection_t	statsCurrentRound;
	StatsCollection_t	statsCurrentMatch;
	KillStats_t		statsKills;
};


struct WeaponName_StatId
{
	CSWeaponID   weaponId;
	CSStatType_t killStatId;
	CSStatType_t shotStatId;
	CSStatType_t hitStatId;
	CSStatType_t damageStatId;
};

struct MapName_MapStatId
{
	char* szMapName;
	CSStatType_t statWinsId;
	CSStatType_t statRoundsId;
	CSStatType_t matchesWonId;
};

extern const MapName_MapStatId MapName_StatId_Table[];

//A mapping from weapon names to weapon stat IDs
extern const WeaponName_StatId WeaponName_StatId_Table[];

//Used to look up the appropriate entry by the ID of the actual weapon
const WeaponName_StatId& GetWeaponTableEntryFromWeaponId(CSWeaponID id);


#endif // CS_GAMESTATS_SHARED_H
