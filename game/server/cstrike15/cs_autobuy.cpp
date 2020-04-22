//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Data for Autobuy and Rebuy 
//
//=============================================================================//

#include "cbase.h"
#include "cs_autobuy.h"

// Weapon class information for each weapon including the class name and the buy command alias.
AutoBuyInfoStruct g_autoBuyInfo[] = 
{
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE0, "galil", "weapon_galil" }, // galil
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE0, "galilar", "weapon_galilar" }, // galilar
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE1, "ak47", "weapon_ak47" },  // ak47
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_INVALID, "scout", "weapon_scout" }, // scout
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_INVALID, "sg552", "weapon_sg552" }, // sg552
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE4, "awp", "weapon_awp" }, // awp
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE5, "g3sg1", "weapon_g3sg1" }, // g3sg1
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE0, "famas", "weapon_famas" }, // famas
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE1, "m4a1", "weapon_m4a1" }, // m4a1
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE1, "m4a1_silencer", "weapon_m4a1_silencer" }, // m4a1_silencer
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE3, "aug", "weapon_aug" }, // aug
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_INVALID, "scar17", "weapon_scar17" }, // scar17
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_RIFLE),			LOADOUT_POSITION_RIFLE3, "sg556", "weapon_sg556" }, // sg556
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_INVALID, "sg550", "weapon_sg550" }, // sg550
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE5, "scar20", "weapon_scar20" }, // scar20
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SNIPERRIFLE),		LOADOUT_POSITION_RIFLE2, "ssg08", "weapon_ssg08" }, // ssg08
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY0, "glock", "weapon_glock" }, // glock
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_INVALID, "usp", "weapon_usp" }, // usp
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_INVALID, "p228", "weapon_p228" }, // p228
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY4, "deagle", "weapon_deagle" }, // deagle
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY1, "elite", "weapon_elite" },	// elites
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY3, "fn57", "weapon_fiveseven" },	// fn57
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY0, "hkp2000", "weapon_hkp2000" },	// hkp2000
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY0, "usp_silencer", "weapon_usp_silencer" },	// usp_silencer
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY2, "p250", "weapon_p250" },	// p250
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_SECONDARY3, "tec9", "weapon_tec9" },	// p250
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_PISTOL),			LOADOUT_POSITION_INVALID, "taser", "weapon_taser" },	// taser
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_HEAVY0, "nova", "weapon_nova" }, // nova
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_HEAVY1, "xm1014", "weapon_xm1014" },	// xm1014
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_HEAVY2, "mag7", "weapon_mag7" },	// mag7
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHOTGUN),			LOADOUT_POSITION_INVALID, "stryker", "weapon_stryker" },	// stryker
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG0, "mac10", "weapon_mac10" },	// mac10
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_INVALID, "tmp", "weapon_tmp" },	// tmp
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_INVALID, "mp5navy", "weapon_mp5navy" },	// mp5navy
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG2, "ump45", "weapon_ump45" },	// ump45
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG3, "p90", "weapon_p90" },	// p90
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG4, "bizon", "weapon_bizon" },	// bizon
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG1, "mp7", "weapon_mp7" },	// mp7
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SMG),				LOADOUT_POSITION_SMG0, "mp9", "weapon_mp9" },	// mp9
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_MACHINEGUN),		LOADOUT_POSITION_HEAVY3, "m249", "weapon_m249" },	// m249
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_MACHINEGUN),		LOADOUT_POSITION_HEAVY4, "negev", "weapon_negev" },	// negev
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_AMMO),				LOADOUT_POSITION_INVALID, "primammo", "primammo" },	// primammo
	{ (AutoBuyClassType)(AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_AMMO),			LOADOUT_POSITION_INVALID, "secammo", "secammo" }, // secmmo
	{ (AutoBuyClassType)(AUTOBUYCLASS_ARMOR),									LOADOUT_POSITION_INVALID, "vest", "item_kevlar" }, // vest
	{ (AutoBuyClassType)(AUTOBUYCLASS_ARMOR),									LOADOUT_POSITION_INVALID, "vesthelm", "item_assaultsuit" }, // vesthelm
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE2, "flashbang", "weapon_flashbang" }, // flash
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE3, "hegrenade", "weapon_hegrenade" }, // hegren
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE4, "smokegrenade", "weapon_smokegrenade" }, // sgren
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE0, "molotov", "weapon_molotov" }, // molotov
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE0, "incgrenade", "weapon_incgrenade" }, // incgrenade
	{ (AutoBuyClassType)(AUTOBUYCLASS_GRENADE),									LOADOUT_POSITION_GRENADE1, "decoy", "weapon_decoy" }, // molotov
	{ (AutoBuyClassType)(AUTOBUYCLASS_NIGHTVISION),								LOADOUT_POSITION_INVALID, "nvgs", "nvgs" }, // nvgs
	{ (AutoBuyClassType)(AUTOBUYCLASS_DEFUSER),									LOADOUT_POSITION_INVALID, "defuser", "defuser" }, // defuser
	{ (AutoBuyClassType)(AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_SHIELD),			LOADOUT_POSITION_INVALID, "shield", "shield" }, // shield

	{ (AutoBuyClassType)0, LOADOUT_POSITION_INVALID, "", "" } // last one, must be at end.
};
