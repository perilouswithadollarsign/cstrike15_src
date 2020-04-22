//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared CS definitions.
//
//=============================================================================//

#ifndef CS_ACHIEVEMENTDEFS_H
#define CS_ACHIEVEMENTDEFS_H
#ifdef _WIN32
#pragma once
#endif



//=============================================================================
// Achievement ID Definitions
//=============================================================================

// set below to 1 to enable
#define ALL_WEARING_SAME_UNIFORM_ACHIEVEMENT 0

typedef enum
{
	CSInvalidAchievement = -1,

	// Bomb-related Achievements
	CSBombAchievementsStart = 1000,        // First bomb-related achievement

	CSWinBombPlant,
	CSWinBombDefuse,
	CSDefuseAndNeededKit,
	CSBombDefuseCloseCall,
	CSKilledDefuser,
	CSPlantBombWithin25Seconds,
	CSKillBombPickup,
	CSBombMultikill,
	CSGooseChase,
	CSWinBombPlantAfterRecovery,
	CSDefuseDefense,
	CSPlantBombsLow,
	CSDefuseBombsLow,
	CSPlantBombsTRLow,
	CSDefuseBombsTRLow,

	CSBombAchievementsEnd,                  // Must be after last bomb-related achievement


	// Hostage-related Achievements
	CSHostageAchievementsStart = 2000,      // First hostage-related achievement

	CSRescueAllHostagesInARound,
	CSKilledRescuer,
	CSFastHostageRescue,
	CSRescueHostagesLow,
	CSRescueHostagesMid,

	CSHostageAchievmentEnd,                 // Must be after last hostage-related achievement

	// General Kill Achievements
	CSKillAchievementsStart = 3000,         // First kill-related achievement

	CSEnemyKillsLow,
	CSEnemyKillsMed,
	CSEnemyKillsHigh,
	CSSurvivedHeadshotDueToHelmet,
	CSKillEnemyReloading,
	CSKillingSpree,
	CSKillsWithMultipleGuns,
	CSHeadshots,
	CSAvengeFriend,
	CSSurviveGrenade,
	CSDominationsLow,
	CSDominationsHigh,
	CSRevengesLow,
	CSRevengesHigh,
	CSDominationOverkillsLow,
	CSDominationOverkillsHigh,
	CSDominationOverkillsMatch,
	CSExtendedDomination,
	CSConcurrentDominations,
	CSKillEnemyBlinded,
	CSKillEnemiesWhileBlind,
	CSKillEnemiesWhileBlindHard,
	CSKillsEnemyWeapon,
	CSKillWithEveryWeapon,
	CSWinKnifeFightsLow,
	CSWinKnifeFightsHigh,
	CSKilledDefuserWithGrenade,
	CSKillSniperWithSniper,
	CSKillSniperWithKnife,
	CSHipShot,
	CSKillSnipers,
	CSKillWhenAtLowHealth,
	CSPistolRoundKnifeKill,
	CSWinDualDuel,
	CSGrenadeMultikill,
	CSKillWhileInAir,
	CSKillEnemyInAir,
	CSKillerAndEnemyInAir,
	CSKillEnemyWithFormerGun,
	CSKillTwoWithOneShot,
	CSProgressiveGameKills,
	CSSelectGameKills,
	CSBombGameKills,
	CSGunGameKillKnifer,
	CSGunGameKnifeSuicide,
	CSGunGameKnifeKillKnifer,
	CSGunGameSMGKillKnifer,
	CSFirstBulletKills,
	CSSpawnCamper,
	CSBornReady,

	CSKillAchievementEnd,                   // Must be after last kill-related achievement

	// Weapon-related Achievements
	CSWeaponAchievementsStart = 4000,       // First weapon-related achievement

	CSEnemyKillsDeagle,
	CSEnemyKillsUSP,
	CSEnemyKillsGlock,
	CSEnemyKillsP228,
	CSEnemyKillsElite,
	CSEnemyKillsFiveSeven,
	CSEnemyKillsBizon,
	CSEnemyKillsTec9,
	CSEnemyKillsTaser,
	CSEnemyKillsHKP2000,
	CSEnemyKillsP250,
	CSEnemyKillsAWP,
	CSEnemyKillsAK47,
	CSEnemyKillsM4A1,
	CSEnemyKillsAUG,
	CSEnemyKillsSG552,
	CSEnemyKillsSG550,
	CSEnemyKillsGALIL,
	CSEnemyKillsGALILAR,
	CSEnemyKillsFAMAS,
	CSEnemyKillsScout,
	CSEnemyKillsG3SG1,
	CSEnemyKillsSCAR17,
	CSEnemyKillsSCAR20,
	CSEnemyKillsSG556,
	CSEnemyKillsSSG08,
	CSEnemyKillsP90,
	CSEnemyKillsMP5NAVY,
	CSEnemyKillsTMP,
	CSEnemyKillsMAC10,
	CSEnemyKillsUMP45,
	CSEnemyKillsMP7,
	CSEnemyKillsMP9,
	CSEnemyKillsM3,
	CSEnemyKillsXM1014,
	CSEnemyKillsMag7,
	CSEnemyKillsSawedoff,
	CSEnemyKillsNova,
	CSEnemyKillsM249,
	CSEnemyKillsNegev,
	CSEnemyKillsKnife,
	CSEnemyKillsHEGrenade,
	CSEnemyKillsMolotov,
	CSMetaPistol,
	CSMetaRifle,
	CSMetaSMG,
	CSMetaShotgun,
	CSMetaWeaponMaster,

	CSWeaponAchievementsEnd,                // Must be after last weapon-related achievement

	// General Achievements
	CSGeneralAchievementsStart = 5000,      // First general achievement

	CSWinRoundsLow,
	CSWinRoundsMed,
	CSWinRoundsHigh,
	CSGGWinRoundsLow,
	CSGGWinRoundsMed,
	CSGGWinRoundsHigh,
	CSGGWinRoundsExtreme,
	CSGGWinRoundsUltimate,
	CSGGRoundsLow,
	CSGGRoundsMed,
	CSGGRoundsHigh,
	CSMoneyEarnedLow,
	CSMoneyEarnedMed,
	CSMoneyEarnedHigh,
	CSGiveDamageLow,
	CSGiveDamageMed,
	CSGiveDamageHigh,
	CSPosthumousGrenadeKill,
	CSKillEnemyTeam,
	CSLastPlayerAlive,
	CSKillEnemyLastBullet,
	CSKillingSpreeEnder,
	CSDamageNoKill,
	CSKillLowDamage,
	CSSurviveManyAttacks,
	CSLosslessExtermination,
	CSFlawlessVictory,
	CSDecalSprays,
	CSBreakWindows,
	CSBreakProps,
	CSUnstoppableForce,
	CSImmovableObject,
	CSHeadshotsInRound,
	CSWinPistolRoundsLow,
	CSWinPistolRoundsMed,
	CSWinPistolRoundsHigh,
	CSFastRoundWin,
	CSNightvisionDamage,
	CSSilentWin,
	CSBloodlessVictory,
	CSDonateWeapons,
	CSWinRoundsWithoutBuying,
#if(ALL_WEARING_SAME_UNIFORM_ACHIEVEMENT)
	CSSameUniform,
#endif
	CSFriendsSameUniform,
	CSCauseFriendlyFireWithFlashbang,

	CSGeneralAchievementsEnd,               // Must be after last general achievement

	CSWinMapAchievementsStart = 6000,

	CSWinMapCS_ASSAULT,
	CSWinMapCS_COMPOUND,
	CSWinMapCS_HAVANA,  
	CSWinMapCS_ITALY,
	CSWinMapCS_MILITIA,
	CSWinMapCS_OFFICE,
	CSWinMapDE_AZTEC,
	CSWinMapDE_CBBLE,
	CSWinMapDE_CHATEAU,
	CSWinMapDE_DUST,
	CSWinMapDE_DUST2,
	CSWinMapDE_INFERNO,
	CSWinMapDE_NUKE,
	CSWinMapDE_PIRANESI,
	CSWinMapDE_PORT,
	CSWinMapDE_PRODIGY,
	CSWinMapDE_TIDES,
	CSWinMapDE_TRAIN,

	CSWinMatchDE_SHORTTRAIN,
	CSWinMatchDE_LAKE,
	CSWinMatchDE_SAFEHOUSE,   
	CSWinMatchDE_SUGARCANE,	
	CSWinMatchDE_STMARC,  
	CSWinMatchDE_BANK,	
	CSWinMatchDE_EMBASSY,
	CSWinMatchDE_DEPOT,
	CSWinMatchDE_VERTIGO,
	CSWinMatchDE_BALKAN,
	CSWinMatchAR_MONASTERY,
	CSWinMatchAR_SHOOTS,
	CSWinMatchAR_BAGGAGE,
	CSWinEveryGGMap,
	CSPlayEveryGGMap,
	CSGunGameProgressiveRampage,
	CSGunGameFirstKill,
	CSKillEnemyTerrTeamBeforeBombPlant,
	CSKillEnemyCTTeamBeforeBombPlant,
	CSGunGameConservationist,
	CSStillAlive,
	CSMedalist,

	CSWinMapAchievementsEnd,                 //Must be after last map-based achievement

	CSSeason1_Start = 6200,
	CSSeason1_Bronze,
	CSSeason1_Silver,
	CSSeason1_Gold,
	CSSeason1_End               // Must be after last season 1 achievement

} eCSAchievementType;


#endif // CS_ACHIEVEMENTDEFS_H
