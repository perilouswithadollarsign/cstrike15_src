//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#ifdef GAME_DLL
#include "GameStats.h"
#endif
#include "cs_gamestats_shared.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// note:  We don't log match stats for any maps with CSSTAT_UNDEFINED for matchID
const MapName_MapStatId MapName_StatId_Table[] =
{
	{"cs_assault",  CSSTAT_MAP_WINS_CS_ASSAULT,		CSSTAT_MAP_ROUNDS_CS_ASSAULT, CSSTAT_UNDEFINED },
	{"cs_italy",    CSSTAT_MAP_WINS_CS_ITALY,		CSSTAT_MAP_ROUNDS_CS_ITALY, CSSTAT_UNDEFINED },
	{"cs_office",   CSSTAT_MAP_WINS_CS_OFFICE,		CSSTAT_MAP_ROUNDS_CS_OFFICE, CSSTAT_UNDEFINED },
	{"cs_militia",  CSSTAT_MAP_WINS_CS_MILITIA,		CSSTAT_MAP_ROUNDS_CS_MILITIA, CSSTAT_UNDEFINED },
	{"de_aztec",    CSSTAT_MAP_WINS_DE_AZTEC,		CSSTAT_MAP_ROUNDS_DE_AZTEC, CSSTAT_UNDEFINED },
	{"de_cbble",    CSSTAT_MAP_WINS_DE_CBBLE,		CSSTAT_MAP_ROUNDS_DE_CBBLE, CSSTAT_UNDEFINED },
	{"de_dust2",    CSSTAT_MAP_WINS_DE_DUST2,		CSSTAT_MAP_ROUNDS_DE_DUST2, CSSTAT_UNDEFINED },
	{"de_dust",     CSSTAT_MAP_WINS_DE_DUST,		CSSTAT_MAP_ROUNDS_DE_DUST, CSSTAT_UNDEFINED },
	{"de_inferno",  CSSTAT_MAP_WINS_DE_INFERNO,		CSSTAT_MAP_ROUNDS_DE_INFERNO, CSSTAT_UNDEFINED },
	{"de_nuke",     CSSTAT_MAP_WINS_DE_NUKE,		CSSTAT_MAP_ROUNDS_DE_NUKE, CSSTAT_UNDEFINED },
	{"de_piranesi", CSSTAT_MAP_WINS_DE_PIRANESI,	CSSTAT_MAP_ROUNDS_DE_PIRANESI, CSSTAT_UNDEFINED },
	{"de_prodigy",  CSSTAT_MAP_WINS_DE_PRODIGY,		CSSTAT_MAP_ROUNDS_DE_PRODIGY, CSSTAT_UNDEFINED },
	{"de_lake",		CSSTAT_MAP_WINS_DE_LAKE,		CSSTAT_MAP_ROUNDS_DE_LAKE, CSSTAT_MAP_MATCHES_WON_LAKE },
	{"de_safehouse",CSSTAT_MAP_WINS_DE_SAFEHOUSE,	CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE, CSSTAT_MAP_MATCHES_WON_SAFEHOUSE},
	{"de_shorttrain",CSSTAT_MAP_WINS_DE_SHORTTRAIN,	CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN, CSSTAT_MAP_MATCHES_WON_SHORTTRAIN }, 
	{"de_sugarcane",CSSTAT_MAP_WINS_DE_SUGARCANE,	CSSTAT_MAP_ROUNDS_DE_SUGARCANE, CSSTAT_MAP_MATCHES_WON_SUGARCANE },
	{"de_stmarc",   CSSTAT_MAP_WINS_DE_STMARC,		CSSTAT_MAP_ROUNDS_DE_STMARC, CSSTAT_MAP_MATCHES_WON_STMARC },
	{"de_bank",     CSSTAT_MAP_WINS_DE_BANK,		CSSTAT_MAP_ROUNDS_DE_BANK, CSSTAT_MAP_MATCHES_WON_BANK },
	{"de_embassy",  CSSTAT_MAP_WINS_DE_EMBASSY,		CSSTAT_MAP_ROUNDS_DE_EMBASSY, CSSTAT_MAP_MATCHES_WON_EMBASSY },
	{"de_depot",    CSSTAT_MAP_WINS_DE_DEPOT,		CSSTAT_MAP_ROUNDS_DE_DEPOT, CSSTAT_MAP_MATCHES_WON_DEPOT },
	{"de_vertigo",  CSSTAT_MAP_WINS_DE_VERTIGO,		CSSTAT_MAP_ROUNDS_DE_VERTIGO, CSSTAT_UNDEFINED},
	{"de_balkan",	CSSTAT_MAP_WINS_DE_BALKAN,		CSSTAT_MAP_ROUNDS_DE_BALKAN, CSSTAT_UNDEFINED},
	{"ar_monastery",CSSTAT_MAP_WINS_AR_MONASTERY,	CSSTAT_MAP_ROUNDS_AR_MONASTERY, CSSTAT_UNDEFINED},
	{"ar_shoots",   CSSTAT_MAP_WINS_AR_SHOOTS,		CSSTAT_MAP_ROUNDS_AR_SHOOTS, CSSTAT_MAP_MATCHES_WON_SHOOTS},
	{"ar_baggage",  CSSTAT_MAP_WINS_AR_BAGGAGE,		CSSTAT_MAP_ROUNDS_AR_BAGGAGE,CSSTAT_MAP_MATCHES_WON_BAGGAGE},
	{"de_train",	CSSTAT_MAP_WINS_DE_TRAIN,		CSSTAT_MAP_ROUNDS_DE_TRAIN,		CSSTAT_MAP_MATCHES_WON_TRAIN }, 
	{"",			CSSTAT_UNDEFINED,				CSSTAT_UNDEFINED, CSSTAT_UNDEFINED },
};

const WeaponName_StatId WeaponName_StatId_Table[] =
{
	{ WEAPON_DEAGLE,		CSSTAT_KILLS_DEAGLE,	CSSTAT_SHOTS_DEAGLE,	CSSTAT_HITS_DEAGLE, 	CSSTAT_DAMAGE_DEAGLE 	},
	{ WEAPON_USP,			CSSTAT_KILLS_USP,		CSSTAT_SHOTS_USP,		CSSTAT_HITS_USP,		CSSTAT_DAMAGE_USP		},
	{ WEAPON_GLOCK,			CSSTAT_KILLS_GLOCK,		CSSTAT_SHOTS_GLOCK,		CSSTAT_HITS_GLOCK,		CSSTAT_DAMAGE_GLOCK		},
	{ WEAPON_P228,			CSSTAT_KILLS_P228,		CSSTAT_SHOTS_P228,		CSSTAT_HITS_P228,		CSSTAT_DAMAGE_P228		},
	{ WEAPON_ELITE,			CSSTAT_KILLS_ELITE,		CSSTAT_SHOTS_ELITE,		CSSTAT_HITS_ELITE,		CSSTAT_DAMAGE_ELITE		},
	{ WEAPON_FIVESEVEN,		CSSTAT_KILLS_FIVESEVEN, CSSTAT_SHOTS_FIVESEVEN, CSSTAT_HITS_FIVESEVEN,	CSSTAT_DAMAGE_FIVESEVEN	},
	{ WEAPON_AWP,			CSSTAT_KILLS_AWP,		CSSTAT_SHOTS_AWP,		CSSTAT_HITS_AWP,		CSSTAT_DAMAGE_AWP		},
	{ WEAPON_AK47,			CSSTAT_KILLS_AK47,		CSSTAT_SHOTS_AK47,		CSSTAT_HITS_AK47,		CSSTAT_DAMAGE_AK47		},
	{ WEAPON_M4A1,			CSSTAT_KILLS_M4A1,		CSSTAT_SHOTS_M4A1,		CSSTAT_HITS_M4A1,		CSSTAT_DAMAGE_M4A1		},
	{ WEAPON_AUG,			CSSTAT_KILLS_AUG,		CSSTAT_SHOTS_AUG,		CSSTAT_HITS_AUG,		CSSTAT_DAMAGE_AUG		},
	{ WEAPON_SG552,			CSSTAT_KILLS_SG552,		CSSTAT_SHOTS_SG552,		CSSTAT_HITS_SG552, 		CSSTAT_DAMAGE_SG552 	},
	{ WEAPON_SG550,			CSSTAT_KILLS_SG550,		CSSTAT_SHOTS_SG550,		CSSTAT_HITS_SG550, 		CSSTAT_DAMAGE_SG550 	},
	{ WEAPON_GALIL,			CSSTAT_KILLS_GALIL,		CSSTAT_SHOTS_GALIL,		CSSTAT_HITS_GALIL, 		CSSTAT_DAMAGE_GALIL 	},
	{ WEAPON_GALILAR,		CSSTAT_KILLS_GALILAR,	CSSTAT_SHOTS_GALILAR,	CSSTAT_HITS_GALILAR, 	CSSTAT_DAMAGE_GALILAR 	},
	{ WEAPON_FAMAS,			CSSTAT_KILLS_FAMAS,		CSSTAT_SHOTS_FAMAS,		CSSTAT_HITS_FAMAS, 		CSSTAT_DAMAGE_FAMAS 	},
	{ WEAPON_SCOUT,			CSSTAT_KILLS_SCOUT,		CSSTAT_SHOTS_SCOUT,		CSSTAT_HITS_SCOUT, 		CSSTAT_DAMAGE_SCOUT 	},
	{ WEAPON_G3SG1,			CSSTAT_KILLS_G3SG1,		CSSTAT_SHOTS_G3SG1,		CSSTAT_HITS_G3SG1, 		CSSTAT_DAMAGE_G3SG1 	},
	{ WEAPON_P90,			CSSTAT_KILLS_P90,		CSSTAT_SHOTS_P90,		CSSTAT_HITS_P90,		CSSTAT_DAMAGE_P90		},
	{ WEAPON_MP5NAVY,		CSSTAT_KILLS_MP5NAVY,	CSSTAT_SHOTS_MP5NAVY,	CSSTAT_HITS_MP5NAVY,	CSSTAT_DAMAGE_MP5NAVY	},
	{ WEAPON_TMP,			CSSTAT_KILLS_TMP,		CSSTAT_SHOTS_TMP,		CSSTAT_HITS_TMP,		CSSTAT_DAMAGE_TMP		},
	{ WEAPON_MAC10,			CSSTAT_KILLS_MAC10,		CSSTAT_SHOTS_MAC10,		CSSTAT_HITS_MAC10,		CSSTAT_DAMAGE_MAC10		},
	{ WEAPON_UMP45,			CSSTAT_KILLS_UMP45,		CSSTAT_SHOTS_UMP45,		CSSTAT_HITS_UMP45,		CSSTAT_DAMAGE_UMP45		},
	{ WEAPON_M3,			CSSTAT_KILLS_M3,		CSSTAT_SHOTS_M3,		CSSTAT_HITS_M3,			CSSTAT_DAMAGE_M3		},
	{ WEAPON_XM1014,		CSSTAT_KILLS_XM1014,	CSSTAT_SHOTS_XM1014,	CSSTAT_HITS_XM1014,		CSSTAT_DAMAGE_XM1014	},
	{ WEAPON_M249,			CSSTAT_KILLS_M249,		CSSTAT_SHOTS_M249,		CSSTAT_HITS_M249,		CSSTAT_DAMAGE_M249		},
	{ WEAPON_KNIFE_GG,		CSSTAT_KILLS_KNIFE,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_KNIFE,			CSSTAT_KILLS_KNIFE,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_HEGRENADE,		CSSTAT_KILLS_HEGRENADE, CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_MOLOTOV,		CSSTAT_KILLS_MOLOTOV,	CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_INCGRENADE,	CSSTAT_KILLS_MOLOTOV,	CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_DECOY,			CSSTAT_KILLS_DECOY,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},
	{ WEAPON_BIZON,			CSSTAT_KILLS_BIZON,		CSSTAT_SHOTS_BIZON,		CSSTAT_HITS_BIZON,		CSSTAT_DAMAGE_BIZON		},
	{ WEAPON_MAG7,			CSSTAT_KILLS_MAG7,		CSSTAT_SHOTS_MAG7,		CSSTAT_HITS_MAG7,		CSSTAT_DAMAGE_MAG7		},
	{ WEAPON_NEGEV,			CSSTAT_KILLS_NEGEV,		CSSTAT_SHOTS_NEGEV,		CSSTAT_HITS_NEGEV,		CSSTAT_DAMAGE_NEGEV		},
	{ WEAPON_SAWEDOFF,		CSSTAT_KILLS_SAWEDOFF,	CSSTAT_SHOTS_SAWEDOFF,	CSSTAT_HITS_SAWEDOFF,	CSSTAT_DAMAGE_SAWEDOFF	},
	{ WEAPON_TEC9,			CSSTAT_KILLS_TEC9,		CSSTAT_SHOTS_TEC9,		CSSTAT_HITS_TEC9,		CSSTAT_DAMAGE_TEC9		},
	{ WEAPON_TASER,			CSSTAT_KILLS_TASER,		CSSTAT_SHOTS_TASER,		CSSTAT_HITS_TASER,		CSSTAT_DAMAGE_TASER		},
	{ WEAPON_HKP2000,		CSSTAT_KILLS_HKP2000,	CSSTAT_SHOTS_HKP2000,	CSSTAT_HITS_HKP2000,	CSSTAT_DAMAGE_HKP2000	},
	{ WEAPON_MP7,			CSSTAT_KILLS_MP7,		CSSTAT_SHOTS_MP7,		CSSTAT_HITS_MP7,		CSSTAT_DAMAGE_MP7		},
	{ WEAPON_MP9,			CSSTAT_KILLS_MP9,		CSSTAT_SHOTS_MP9,		CSSTAT_HITS_MP9,		CSSTAT_DAMAGE_MP9		},
	{ WEAPON_NOVA,			CSSTAT_KILLS_NOVA,		CSSTAT_SHOTS_NOVA,		CSSTAT_HITS_NOVA,		CSSTAT_DAMAGE_NOVA		},
	{ WEAPON_P250,			CSSTAT_KILLS_P250,		CSSTAT_SHOTS_P250,		CSSTAT_HITS_P250,		CSSTAT_DAMAGE_P250		},
	{ WEAPON_SCAR17,		CSSTAT_KILLS_SCAR17,	CSSTAT_SHOTS_SCAR17,	CSSTAT_HITS_SCAR17,		CSSTAT_DAMAGE_SCAR17	},
	{ WEAPON_SCAR20,		CSSTAT_KILLS_SCAR20,	CSSTAT_SHOTS_SCAR20,	CSSTAT_HITS_SCAR20,		CSSTAT_DAMAGE_SCAR20	},
	{ WEAPON_SG556,			CSSTAT_KILLS_SG556,		CSSTAT_SHOTS_SG556,		CSSTAT_HITS_SG556,		CSSTAT_DAMAGE_SG556		},
	{ WEAPON_SSG08,			CSSTAT_KILLS_SSG08,		CSSTAT_SHOTS_SSG08,		CSSTAT_HITS_SSG08,		CSSTAT_DAMAGE_SSG08		},
	{ WEAPON_NONE,			CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED		},			// This is a sentinel value so we can loop through all the stats
//	{ WEAPON_SENSORGRENADE, CSSTAT_KILLS_SENSOR,	CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED,		CSSTAT_UNDEFINED },

};


struct CSStatProperty_Init
{
	int statId;
	CSStatProperty	statProperty;
// 	const char*	szSteamName;			// name of the stat on steam
// 	const char*	szLocalizationToken;	// localization token for the stat
// 	uint flags;							// priority flags for sending to client
};

CSStatProperty_Init CSStatProperty_Table_Init[] =
{
//		StatId									Steam Name							Localization Token						Client Update Priority
	{	CSSTAT_SHOTS_HIT,						"total_shots_hit",					"#GAMEUI_Stat_NumHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FIRED,						"total_shots_fired",				"#GAMEUI_Stat_NumShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_KILLS,							"total_kills",						"#GAMEUI_Stat_NumKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DEATHS,							"total_deaths",						"#GAMEUI_Stat_NumDeaths",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DAMAGE,							"total_damage_done",				"#GAMEUI_Stat_DamageDone",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BOMBS_PLANTED,				"total_planted_bombs",				"#GAMEUI_Stat_NumPlantedBombs",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BOMBS_DEFUSED,				"total_defused_bombs",				"#GAMEUI_Stat_NumDefusedBombs",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_TR_NUM_BOMBS_PLANTED,			"total_TR_planted_bombs",			"#GAMEUI_Stat_TR_NumPlantedBombs",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_TR_NUM_BOMBS_DEFUSED,			"total_TR_defused_bombs",			"#GAMEUI_Stat_TR_NumDefusedBombs",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PLAYTIME,						"total_time_played",				"#GAMEUI_Stat_TimePlayed",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_ROUNDS_WON,						"total_wins",						"#GAMEUI_Stat_TotalWins",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_T_ROUNDS_WON,					NULL,								NULL,									CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_CT_ROUNDS_WON,					NULL,								NULL,									CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_ROUNDS_PLAYED,					"total_rounds_played",				"#GAMEUI_Stat_TotalRounds",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_PISTOLROUNDS_WON,				"total_wins_pistolround",			"#GAMEUI_Stat_PistolRoundWins",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSTAT_GUNGAME_ROUNDS_WON,				"total_gun_game_rounds_won",		"#GAMEUI_Stat_gun_game_rounds_won",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSTAT_GUNGAME_ROUNDS_PLAYED,			"total_gun_game_rounds_played",		"#GAMEUI_Stat_gun_game_rounds_played",  CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MONEY_EARNED,					"total_money_earned",				"#GAMEUI_Stat_MoneyEarned",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_OBJECTIVES_COMPLETED,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_BOMBS_DEFUSED_WITHKIT,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_DEAGLE,					"total_kills_deagle",				"#GAMEUI_Stat_DeagleKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_USP,						"total_kills_usp",					"#GAMEUI_Stat_USPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GLOCK,						"total_kills_glock",				"#GAMEUI_Stat_GlockKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P228,						"total_kills_p228",					"#GAMEUI_Stat_P228Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_ELITE,						"total_kills_elite",				"#GAMEUI_Stat_EliteKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_FIVESEVEN,					"total_kills_fiveseven",			"#GAMEUI_Stat_FiveSevenKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AWP,						"total_kills_awp",					"#GAMEUI_Stat_AWPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AK47,						"total_kills_ak47",					"#GAMEUI_Stat_AK47Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M4A1,						"total_kills_m4a1",					"#GAMEUI_Stat_M4A1Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_AUG,						"total_kills_aug",					"#GAMEUI_Stat_AUGKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG552,						"total_kills_sg552",				"#GAMEUI_Stat_SG552Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG550,						"total_kills_sg550",				"#GAMEUI_Stat_SG550Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GALIL,						"total_kills_galil",				"#GAMEUI_Stat_GALILKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_GALILAR,					"total_kills_galilar",				"#GAMEUI_Stat_GALILARKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_FAMAS,						"total_kills_famas",				"#GAMEUI_Stat_FAMASKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCOUT,						"total_kills_scout",				"#GAMEUI_Stat_ScoutKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_G3SG1,						"total_kills_g3sg1",				"#GAMEUI_Stat_G3SG1Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P90,						"total_kills_p90",					"#GAMEUI_Stat_P90Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP5NAVY,					"total_kills_mp5navy",				"#GAMEUI_Stat_MP5NavyKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TMP,						"total_kills_tmp",					"#GAMEUI_Stat_TMPKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MAC10,						"total_kills_mac10",				"#GAMEUI_Stat_MAC10Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_UMP45,						"total_kills_ump45",				"#GAMEUI_Stat_UMP45Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M3,						"total_kills_m3",					"#GAMEUI_Stat_M3Kills",					CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_XM1014,					"total_kills_xm1014",				"#GAMEUI_Stat_XM1014Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_M249,						"total_kills_m249",					"#GAMEUI_Stat_M249Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_KNIFE,						"total_kills_knife",				"#GAMEUI_Stat_KnifeKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_HEGRENADE,					"total_kills_hegrenade",			"#GAMEUI_Stat_HEGrenadeKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MOLOTOV,					"total_kills_molotov",				"#GAMEUI_Stat_MolotovKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_DECOY,						"total_kills_decoy",				"#GAMEUI_Stat_DecoyKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_BIZON,						"total_kills_bizon",				"#GAMEUI_Stat_BIZONKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MAG7,						"total_kills_mag7",					"#GAMEUI_Stat_MAG7Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_NEGEV,						"total_kills_negev",				"#GAMEUI_Stat_NEGEVKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SAWEDOFF,					"total_kills_sawedoff",				"#GAMEUI_Stat_SAWEDOFFKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TEC9,						"total_kills_tec9",					"#GAMEUI_Stat_TEC9Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_TASER,						"total_kills_taser",				"#GAMEUI_Stat_TASERKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_HKP2000,					"total_kills_hkp2000",				"#GAMEUI_Stat_HKP2000Kills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP7,						"total_kills_mp7",					"#GAMEUI_Stat_MP7Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_MP9,						"total_kills_mp9",					"#GAMEUI_Stat_MP9Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_NOVA,						"total_kills_nova",					"#GAMEUI_Stat_NovaKills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_P250,						"total_kills_p250",					"#GAMEUI_Stat_P250Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCAR17,					"total_kills_scar17",				"#GAMEUI_Stat_SCAR17Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SCAR20,					"total_kills_scar20",				"#GAMEUI_Stat_SCAR20Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SG556,						"total_kills_sg556",				"#GAMEUI_Stat_SG556Kills",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_SSG08,						"total_kills_ssg08",				"#GAMEUI_Stat_SSG08Kills",				CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_SHOTS_DEAGLE,					"total_shots_deagle",				"#GAMEUI_Stat_DeagleShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_USP,						"total_shots_usp",					"#GAMEUI_Stat_USPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GLOCK,						"total_shots_glock",				"#GAMEUI_Stat_GlockShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P228,						"total_shots_p228",					"#GAMEUI_Stat_P228Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_ELITE,						"total_shots_elite",				"#GAMEUI_Stat_EliteShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FIVESEVEN,					"total_shots_fiveseven",			"#GAMEUI_Stat_FiveSevenShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AWP,						"total_shots_awp",					"#GAMEUI_Stat_AWPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AK47,						"total_shots_ak47",					"#GAMEUI_Stat_AK47Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M4A1,						"total_shots_m4a1",					"#GAMEUI_Stat_M4A1Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_AUG,						"total_shots_aug",					"#GAMEUI_Stat_AUGShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG552,						"total_shots_sg552",				"#GAMEUI_Stat_SG552Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG550,						"total_shots_sg550",				"#GAMEUI_Stat_SG550Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GALIL,						"total_shots_galil",				"#GAMEUI_Stat_GALILShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_GALILAR,					"total_shots_galilar",				"#GAMEUI_Stat_GALILARShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_FAMAS,						"total_shots_famas",				"#GAMEUI_Stat_FAMASShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCOUT,						"total_shots_scout",				"#GAMEUI_Stat_ScoutShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_G3SG1,						"total_shots_g3sg1",				"#GAMEUI_Stat_G3SG1Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P90,						"total_shots_p90",					"#GAMEUI_Stat_P90Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP5NAVY,					"total_shots_mp5navy",				"#GAMEUI_Stat_MP5NavyShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TMP,						"total_shots_tmp",					"#GAMEUI_Stat_TMPShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MAC10,						"total_shots_mac10",				"#GAMEUI_Stat_MAC10Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_UMP45,						"total_shots_ump45",				"#GAMEUI_Stat_UMP45Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M3,						"total_shots_m3",					"#GAMEUI_Stat_M3Shots",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_XM1014,					"total_shots_xm1014",				"#GAMEUI_Stat_XM1014Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_M249,						"total_shots_m249",					"#GAMEUI_Stat_M249Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_BIZON,						"total_shots_bizon",				"#GAMEUI_Stat_BIZONShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MAG7,						"total_shots_mag7",					"#GAMEUI_Stat_MAG7Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_NEGEV,						"total_shots_negev",				"#GAMEUI_Stat_NEGEVShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SAWEDOFF,					"total_shots_sawedoff",				"#GAMEUI_Stat_SAWEDOFFShots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TEC9,						"total_shots_tec9",					"#GAMEUI_Stat_TEC9Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_TASER,						"total_shots_taser",				"#GAMEUI_Stat_TASERShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_HKP2000,					"total_shots_hkp2000",				"#GAMEUI_Stat_HKP2000Shots",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP7,						"total_shots_mp7",					"#GAMEUI_Stat_MP7Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_MP9,						"total_shots_mp9",					"#GAMEUI_Stat_MP9Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_NOVA,						"total_shots_nova",					"#GAMEUI_Stat_NovaShots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_P250,						"total_shots_p250",					"#GAMEUI_Stat_P250Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCAR17,					"total_shots_scar17",				"#GAMEUI_Stat_SCAR17Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SCAR20,					"total_shots_scar20",				"#GAMEUI_Stat_SCAR20Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SG556,						"total_shots_sg556",				"#GAMEUI_Stat_SG556Shots",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_SHOTS_SSG08,						"total_shots_ssg08",				"#GAMEUI_Stat_SSG08Shots",				CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_HITS_DEAGLE,						"total_hits_deagle",				"#GAMEUI_Stat_DeagleHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_USP,						"total_hits_usp",					"#GAMEUI_Stat_USPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GLOCK,						"total_hits_glock",					"#GAMEUI_Stat_GlockHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P228,						"total_hits_p228",					"#GAMEUI_Stat_P228Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_ELITE,						"total_hits_elite",					"#GAMEUI_Stat_EliteHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_FIVESEVEN,					"total_hits_fiveseven",				"#GAMEUI_Stat_FiveSevenHits",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AWP,						"total_hits_awp",					"#GAMEUI_Stat_AWPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AK47,						"total_hits_ak47",					"#GAMEUI_Stat_AK47Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M4A1,						"total_hits_m4a1",					"#GAMEUI_Stat_M4A1Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_AUG,						"total_hits_aug",					"#GAMEUI_Stat_AUGHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG552,						"total_hits_sg552",					"#GAMEUI_Stat_SG552Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG550,						"total_hits_sg550",					"#GAMEUI_Stat_SG550Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GALIL,						"total_hits_galil",					"#GAMEUI_Stat_GALILHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_GALILAR,					"total_hits_galilar",				"#GAMEUI_Stat_GALILARHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_FAMAS,						"total_hits_famas",					"#GAMEUI_Stat_FAMASHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCOUT,						"total_hits_scout",					"#GAMEUI_Stat_ScoutHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_G3SG1,						"total_hits_g3sg1",					"#GAMEUI_Stat_G3SG1Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P90,						"total_hits_p90",					"#GAMEUI_Stat_P90Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP5NAVY,					"total_hits_mp5navy",				"#GAMEUI_Stat_MP5NavyHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TMP,						"total_hits_tmp",					"#GAMEUI_Stat_TMPHits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MAC10,						"total_hits_mac10",					"#GAMEUI_Stat_MAC10Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_UMP45,						"total_hits_ump45",					"#GAMEUI_Stat_UMP45Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M3,							"total_hits_m3",					"#GAMEUI_Stat_M3Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_XM1014,						"total_hits_xm1014",				"#GAMEUI_Stat_XM1014Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_M249,						"total_hits_m249",					"#GAMEUI_Stat_M249Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_BIZON,						"total_hits_bizon",					"#GAMEUI_Stat_BIZONHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MAG7,						"total_hits_mag7",					"#GAMEUI_Stat_MAG7Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_NEGEV,						"total_hits_negev",					"#GAMEUI_Stat_NEGEVHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SAWEDOFF,					"total_hits_sawedoff",				"#GAMEUI_Stat_SAWEDOFFHits",			CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TEC9,						"total_hits_tec9",					"#GAMEUI_Stat_TEC9Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_TASER,						"total_hits_taser",					"#GAMEUI_Stat_TASERHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_HKP2000,					"total_hits_hkp2000",				"#GAMEUI_Stat_HKP2000Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP7,						"total_hits_mp7",					"#GAMEUI_Stat_MP7Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_MP9,						"total_hits_mp9",					"#GAMEUI_Stat_MP9Hits",					CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_NOVA,						"total_hits_nova",					"#GAMEUI_Stat_NovaHits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_P250,						"total_hits_p250",					"#GAMEUI_Stat_P250Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCAR17,						"total_hits_scar17",				"#GAMEUI_Stat_SCAR17Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SCAR20,						"total_hits_scar20",				"#GAMEUI_Stat_SCAR20Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SG556,						"total_hits_sg556",					"#GAMEUI_Stat_SG556Hits",				CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_HITS_SSG08,						"total_hits_ssg08",					"#GAMEUI_Stat_SSG08Hits",				CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_DAMAGE_DEAGLE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_USP,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GLOCK,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P228,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_ELITE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_FIVESEVEN,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AWP,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AK47,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M4A1,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_AUG,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG552,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG550,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GALIL,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_GALILAR,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_FAMAS,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCOUT,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_G3SG1,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P90,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP5NAVY,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TMP,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MAC10,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_UMP45,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M3,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_XM1014,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_M249,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_BIZON,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MAG7,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_NEGEV,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SAWEDOFF,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TEC9,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_TASER,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_HKP2000,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP7,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_MP9,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_NOVA,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_P250,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCAR17,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SCAR20,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SG556,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_DAMAGE_SSG08,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_HEADSHOT,					"total_kills_headshot",				"#GAMEUI_Stat_HeadshotKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_ENEMY_BLINDED,				"total_kills_enemy_blinded",		"#GAMEUI_Stat_BlindedEnemyKills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_WHILE_BLINDED,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WITH_LAST_ROUND,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_ENEMY_WEAPON,				"total_kills_enemy_weapon",			"#GAMEUI_Stat_EnemyWeaponKills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_KNIFE_FIGHT,				"total_kills_knife_fight",			"#GAMEUI_Stat_KnifeFightKills",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_KILLS_WHILE_DEFENDING_BOMB,		NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WITH_STATTRAK_WEAPON,		NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_DECAL_SPRAYS,					"total_decal_sprays",				"#GAMEUI_Stat_DecalSprays",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_TOTAL_JUMPS,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE,	NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_KILLS_ENEMY_WOUNDED,				NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_FALL_DAMAGE,						NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_NUM_HOSTAGES_RESCUED,			"total_rescued_hostages",			"#GAMEUI_Stat_NumRescuedHostages",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_NUM_BROKEN_WINDOWS,				"total_broken_windows",				"#GAMEUI_Stat_NumBrokenWindows",		CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_ALL,					NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_MELON,				NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_OFFICEELECTRONICS,	NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_PROPSBROKEN_OFFICERADIO,			NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},
	{	CSSTAT_PROPSBROKEN_OFFICEJUNK,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_PROPSBROKEN_ITALY_MELON,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,		"total_kills_against_zoomed_sniper","#GAMEUI_Stat_ZoomedSniperKills",		CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_WEAPONS_DONATED,					"total_weapons_donated",			"#GAMEUI_Stat_WeaponsDonated",			CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_ITEMS_PURCHASED,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_MONEY_SPENT,						NULL,								NULL,									CSSTAT_PRIORITY_LOW,			},

	{	CSSTAT_DOMINATIONS,						"total_dominations",				"#GAMEUI_Stat_Dominations",				CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_DOMINATION_OVERKILLS,			"total_domination_overkills",		"#GAMEUI_Stat_DominationOverkills",		CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_REVENGES,						"total_revenges",					"#GAMEUI_Stat_Revenges",				CSSTAT_PRIORITY_HIGH,			},

	{	CSSTAT_MVPS,							"total_mvps",						"#GAMEUI_Stat_MVPs",					CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_CONTRIBUTION_SCORE,				"total_contribution_score",			"#GAMEUI_Contribution_Score",			CSSTAT_PRIORITY_HIGH,			},
	{	CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE,"total_gun_game_contribution_score",	"#GAMEUI_GG_Contribution_Score",	CSSTAT_PRIORITY_HIGH,			},
	
	{	CSSTAT_GRENADE_DAMAGE,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_GRENADE_POSTHUMOUSKILLS,			NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSSTAT_GRENADES_THROWN,					NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},
	{	CSTAT_ITEMS_DROPPED_VALUE,              NULL,								NULL,									CSSTAT_PRIORITY_NEVER,			},

	{	CSSTAT_MAP_WINS_CS_ASSAULT,				"total_wins_map_cs_assault",		"#GAMEUI_Stat_WinsMapCSAssault",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_MILITIA,				"total_wins_map_cs_militia",		"#GAMEUI_Stat_WinsMapCSMilitia",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_ITALY,				"total_wins_map_cs_italy",			"#GAMEUI_Stat_WinsMapCSItaly",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_CS_OFFICE,				"total_wins_map_cs_office",			"#GAMEUI_Stat_WinsMapCSOffice",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_AZTEC,				"total_wins_map_de_aztec",			"#GAMEUI_Stat_WinsMapDEAztec",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_CBBLE,				"total_wins_map_de_cbble",			"#GAMEUI_Stat_WinsMapDECbble",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DUST2,				"total_wins_map_de_dust2",			"#GAMEUI_Stat_WinsMapDEDust2",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DUST,				"total_wins_map_de_dust",			"#GAMEUI_Stat_WinsMapDEDust",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_INFERNO,				"total_wins_map_de_inferno",		"#GAMEUI_Stat_WinsMapDEInferno",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_NUKE,				"total_wins_map_de_nuke",			"#GAMEUI_Stat_WinsMapDENuke",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PIRANESI,			"total_wins_map_de_piranesi",		"#GAMEUI_Stat_WinsMapDEPiranesi",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_PRODIGY,				"total_wins_map_de_prodigy",		"#GAMEUI_Stat_WinsMapDEProdigy",		CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_WINS_DE_LAKE,				"total_wins_map_de_lake",			"#GAMEUI_Stat_WinsMapDELake",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_SAFEHOUSE,			"total_wins_map_de_safehouse",		"#GAMEUI_Stat_WinsMapDESafeHouse",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_SHORTTRAIN,			"total_wins_map_de_shorttrain",		"#GAMEUI_Stat_WinsMapShorttrain",		CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_WINS_DE_TRAIN,				"total_wins_map_de_train",			"#GAMEUI_Stat_WinsMapTrain",			CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_WINS_DE_SUGARCANE,			"total_wins_map_de_sugarcane",		"#GAMEUI_Stat_WinsMapDESugarcane",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_STMARC,				"total_wins_map_de_stmarc",			"#GAMEUI_Stat_WinsMapDEStMarc",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_BANK,				"total_wins_map_de_bank",			"#GAMEUI_Stat_WinsMapDEBank",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_EMBASSY,				"total_wins_map_de_embassy",		"#GAMEUI_Stat_WinsMapDEEmbassy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_DEPOT,				"total_wins_map_de_depot",			"#GAMEUI_Stat_WinsMapDEDepot",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_VERTIGO,				"total_wins_map_de_vertigo",        "#GAMEUI_Stat_WinsMapDEVertigo",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_DE_BALKAN,				"total_wins_map_de_balkan",			"#GAMEUI_Stat_WinsMapDEBalkan",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_AR_MONASTERY,			"total_wins_map_ar_monastery",		"#GAMEUI_Stat_WinsMapARMonastery",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_AR_SHOOTS,				"total_wins_map_ar_shoots",         "#GAMEUI_Stat_WinsMapARShoots",	    	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_WINS_AR_BAGGAGE,				"total_wins_map_ar_baggage",        "#GAMEUI_Stat_WinsMapARBaggage",		CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_ROUNDS_CS_MILITIA,			"total_rounds_map_cs_militia",		"#GAMEUI_Stat_RoundsMapCSMilitia",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_ASSAULT,			"total_rounds_map_cs_assault",		"#GAMEUI_Stat_RoundsMapCSAssault",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_ITALY,				"total_rounds_map_cs_italy",		"#GAMEUI_Stat_RoundsMapCSItaly",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_CS_OFFICE,			"total_rounds_map_cs_office",		"#GAMEUI_Stat_RoundsMapCSOffice",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_AZTEC,				"total_rounds_map_de_aztec",		"#GAMEUI_Stat_RoundsMapDEAztec",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_CBBLE,				"total_rounds_map_de_cbble",		"#GAMEUI_Stat_RoundsMapDECbble",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DUST2,				"total_rounds_map_de_dust2",		"#GAMEUI_Stat_RoundsMapDEDust2",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DUST,				"total_rounds_map_de_dust",			"#GAMEUI_Stat_RoundsMapDEDust",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_INFERNO,			"total_rounds_map_de_inferno",		"#GAMEUI_Stat_RoundsMapDEInferno",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_NUKE,				"total_rounds_map_de_nuke",			"#GAMEUI_Stat_RoundsMapDENuke",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PIRANESI,			"total_rounds_map_de_piranesi",		"#GAMEUI_Stat_RoundsMapDEPiranesi",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_PRODIGY,			"total_rounds_map_de_prodigy",		"#GAMEUI_Stat_RoundsMapDEProdigy",		CSSTAT_PRIORITY_ENDROUND,		},

	{	CSSTAT_MAP_ROUNDS_DE_LAKE,				"total_rounds_map_de_lake",			"#GAMEUI_Stat_RoundsMapDELake",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_SAFEHOUSE,			"total_rounds_map_de_safehouse",	"#GAMEUI_Stat_RoundsMapDESafeHouse",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_SHORTTRAIN,		"total_rounds_map_de_shorttrain",	"#GAMEUI_Stat_RoundsMapShorttrain",		CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_ROUNDS_DE_TRAIN,				"total_rounds_map_de_train",		"#GAMEUI_Stat_RoundsMapTrain",			CSSTAT_PRIORITY_ENDROUND,		},	
	{	CSSTAT_MAP_ROUNDS_DE_SUGARCANE,			"total_rounds_map_de_sugarcane",	"#GAMEUI_Stat_RoundsMapDESugarcane",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_STMARC,			"total_rounds_map_de_stmarc",		"#GAMEUI_Stat_RoundsMapDEStMarc",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_BANK,				"total_rounds_map_de_bank",			"#GAMEUI_Stat_RoundsMapDEBank",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_EMBASSY,			"total_rounds_map_de_embassy",		"#GAMEUI_Stat_RoundsMapDEEmbassy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_DEPOT,				"total_rounds_map_de_depot",		"#GAMEUI_Stat_RoundsMapDEDepot",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_VERTIGO,			"total_rounds_map_de_vertigo",		"#GAMEUI_Stat_RoundsMapDEVertigo",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_DE_BALKAN,			"total_rounds_map_de_balkan",		"#GAMEUI_Stat_RoundsMapDEBalkan",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_AR_MONASTERY,			"total_rounds_map_ar_monastery",	"#GAMEUI_Stat_RoundsMapARMonastery",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_AR_SHOOTS,			"total_rounds_map_ar_shoots",		"#GAMEUI_Stat_RoundsMapARShoots",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_ROUNDS_AR_BAGGAGE,			"total_rounds_map_ar_baggage",		"#GAMEUI_Stat_RoundsMapAR_Baggage",		CSSTAT_PRIORITY_ENDROUND,		},
	
	// match stats
	{	CSSTAT_MAP_MATCHES_WON_SHOOTS,			"total_matches_won_shoots",			"#GAMEUI_Stat_MatchWinsShoots",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_BAGGAGE,			"total_matches_won_baggage",		"#GAMEUI_Stat_MatchWinsBaggage",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_LAKE,			"total_matches_won_lake",			"#GAMEUI_Stat_MatchWinsLake",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_SUGARCANE,		"total_matches_won_sugarcane",		"#GAMEUI_Stat_MatchWinsSugarcane",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_STMARC,			"total_matches_won_stmarc",			"#GAMEUI_Stat_MatchWinsStMarc",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_BANK,			"total_matches_won_bank",			"#GAMEUI_Stat_MatchWinsBank",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_EMBASSY,			"total_matches_won_embassy",		"#GAMEUI_Stat_MatchWinsEmbassy",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_DEPOT,			"total_matches_won_depot",			"#GAMEUI_Stat_MatchWinsDepot",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_SAFEHOUSE,		"total_matches_won_safehouse",		"#GAMEUI_Stat_MatchWinsSafeHouse",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_SHORTTRAIN,		"total_matches_won_shorttrain",		"#GAMEUI_Stat_MatchWinsShorttrain",		CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MAP_MATCHES_WON_TRAIN,			"total_matches_won_train",			"#GAMEUI_Stat_MatchWinsTrain",			CSSTAT_PRIORITY_ENDROUND,		},
	

	{	CSSTAT_MATCHES_WON,						"total_matches_won",				"#GAMEUI_Stat_MatchWins",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MATCHES_DRAW,					"total_matches_drawn",				"#GAMEUI_Stat_MatchDraws",				CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_MATCHES_PLAYED,					"total_matches_played",				"#GAMEUI_Stat_MatchesPlayed",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_GUN_GAME_MATCHES_WON,			"total_gg_matches_won",				"#GAMEUI_Stat_MatchWinsGG",				CSSTAT_PRIORITY_ENDROUND,		},
	{   CSSTAT_GUN_GAME_MATCHES_PLAYED,			"total_gg_matches_played",			"GAMEUI_Stat_MatchesPlayedGG",			CSSTAT_PRIORITY_ENDROUND		},
	{	CSSTAT_GUN_GAME_PROGRESSIVE_MATCHES_WON,"total_progressive_matches_won",	"#GAMEUI_Stat_MatchWinsProgressive",	CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_GUN_GAME_SELECT_MATCHES_WON,		"total_select_matches_won",			"#GAMEUI_Stat_MatchWinsSelect",			CSSTAT_PRIORITY_ENDROUND,		},
	{	CSSTAT_GUN_GAME_TRBOMB_MATCHES_WON,		"total_trbomb_matches_won",			"#GAMEUI_Stat_MatchWinsTRBomb",			CSSTAT_PRIORITY_ENDROUND,		},


	// only client tracks these

	
	{	CSSTAT_LASTMATCH_CONTRIBUTION_SCORE,	"last_match_contribution_score",	"#GameUI_Stat_LastMatch_Contrib_Score",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_GG_PROGRESSIVE_CONTRIBUTION_SCORE,"last_match_gg_contribution_score","#GameUI_Stat_LastMatch_gg_Contrib_Score",CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_T_ROUNDS_WON,			"last_match_t_wins",				"#GameUI_Stat_LastMatch_TWins",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_CT_ROUNDS_WON,			"last_match_ct_wins",				"#GameUI_Stat_LastMatch_CTWins",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_ROUNDS_WON,			"last_match_wins",					"#GameUI_Stat_LastMatch_RoundsWon",		CSSTAT_PRIORITY_NEVER },
	{	CSTAT_LASTMATCH_ROUNDS_PLAYED,			"last_match_rounds",				"#GameUI_Stat_LastMatch_RoundsPlayed",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_KILLS,					"last_match_kills",					"#GameUI_Stat_LastMatch_Kills",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DEATHS,				"last_match_deaths",				"#GameUI_Stat_LastMatch_Deaths",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MVPS,					"last_match_mvps",					"#GameUI_Stat_LastMatch_MVPS",			CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DAMAGE,				"last_match_damage",				"#GameUI_Stat_LastMatch_Damage",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MONEYSPENT,			"last_match_money_spent",			"#GameUI_Stat_LastMatch_MoneySpent",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_DOMINATIONS,			"last_match_dominations",			"#GameUI_Stat_LastMatch_Dominations",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_REVENGES,				"last_match_revenges",				"#GameUI_Stat_LastMatch_Revenges",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_MAX_PLAYERS,			"last_match_max_players",			"#GameUI_Stat_LastMatch_MaxPlayers",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_ID,			"last_match_favweapon_id",			"#GameUI_Stat_LastMatch_FavWeapon",		CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,		"last_match_favweapon_shots",		"#GameUI_Stat_LastMatch_FavWeaponShots",CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_HITS,		"last_match_favweapon_hits",		"#GameUI_Stat_LastMatch_FavWeaponHits",	CSSTAT_PRIORITY_NEVER },
	{	CSSTAT_LASTMATCH_FAVWEAPON_KILLS,		"last_match_favweapon_kills",		"#GameUI_Stat_LastMatch_FavWeaponKills",CSSTAT_PRIORITY_NEVER },

	{	CSSTAT_UNDEFINED	},	// sentinel
};


CSStatProperty CSStatProperty_Table[CSSTAT_MAX];

class StatPropertyTableInitializer
{
public:
	StatPropertyTableInitializer()
	{
		memset( CSStatProperty_Table, 0, sizeof( CSStatProperty_Table ) );
		for ( int i = 0; i < ARRAYSIZE( CSStatProperty_Table_Init ); ++i )
		{
			int statId = CSStatProperty_Table_Init[ i ].statId;
			// The last entry in the table_init array is a sentinel. We were using that as
			// a statId causing a 100% buffer underflow, leading to nasty problems.
			if ( statId >= 0 && statId < ARRAYSIZE( CSStatProperty_Table ) )
				CSStatProperty_Table[ statId ] = CSStatProperty_Table_Init[ i ].statProperty;
		}
	}
};

static StatPropertyTableInitializer statPropertyTableInitializer;


const WeaponName_StatId& GetWeaponTableEntryFromWeaponId( CSWeaponID id )
{
	int i;

	//yes this for loop has no statement block. All we are doing is incrementing i to the appropriate point.
	for (i = 0 ; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE ; ++i)
	{
		if (WeaponName_StatId_Table[i].weaponId == id )
		{
			break;
		}
	}
	return WeaponName_StatId_Table[i];
}

void StatsCollection_t::Aggregate( const StatsCollection_t& other )
{
	for ( int i = 0; i < CSSTAT_MAX; ++i )
	{
		m_iValue[i] += other[i];
	}
}
