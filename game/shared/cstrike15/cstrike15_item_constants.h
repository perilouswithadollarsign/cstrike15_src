//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef CSTRIKE15_ITEM_CONSTANTS_H
#define CSTRIKE15_ITEM_CONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Slots for items within loadouts
// NOTE: Explicitly numbered slots are for shipped features with the number saved in the database, do not renumber.
// Some legacy entries in the enum are still here and some game code uses them, but are not yet shipped and would be 
// safe to renumber or remove if needed
//-----------------------------------------------------------------------------
enum loadout_positions_t
{
	LOADOUT_POSITION_INVALID		= -1,

	LOADOUT_POSITION_MELEE			= 0,
	LOADOUT_POSITION_C4				= 1,
	LOADOUT_POSITION_FIRST_AUTO_BUY_WEAPON = LOADOUT_POSITION_MELEE,
	LOADOUT_POSITION_LAST_AUTO_BUY_WEAPON = LOADOUT_POSITION_C4,

	LOADOUT_POSITION_SECONDARY0 = 2,
	LOADOUT_POSITION_SECONDARY1 = 3,
	LOADOUT_POSITION_SECONDARY2 = 4,
	LOADOUT_POSITION_SECONDARY3 = 5,
	LOADOUT_POSITION_SECONDARY4 = 6,
	LOADOUT_POSITION_SECONDARY5 = 7, // Unused, no instances in database yet

	LOADOUT_POSITION_SMG0 = 8,
	LOADOUT_POSITION_SMG1 = 9,
	LOADOUT_POSITION_SMG2 = 10,
	LOADOUT_POSITION_SMG3 = 11,
	LOADOUT_POSITION_SMG4 = 12,
	LOADOUT_POSITION_SMG5 = 13, // Unused, no instances in database yet

	LOADOUT_POSITION_RIFLE0 = 14,
	LOADOUT_POSITION_RIFLE1 = 15,
	LOADOUT_POSITION_RIFLE2 = 16,
	LOADOUT_POSITION_RIFLE3 = 17,
	LOADOUT_POSITION_RIFLE4 = 18,
	LOADOUT_POSITION_RIFLE5 = 19,

	LOADOUT_POSITION_HEAVY0 = 20,
	LOADOUT_POSITION_HEAVY1 = 21,
	LOADOUT_POSITION_HEAVY2 = 22,
	LOADOUT_POSITION_HEAVY3 = 23,
	LOADOUT_POSITION_HEAVY4 = 24,
	LOADOUT_POSITION_HEAVY5	= 25, // Unused, no instances in database yet
	LOADOUT_POSITION_FIRST_WHEEL_WEAPON = LOADOUT_POSITION_SECONDARY0,
	LOADOUT_POSITION_LAST_WHEEL_WEAPON = LOADOUT_POSITION_HEAVY5,


	// Grenade slots not yet saved in db
	LOADOUT_POSITION_FIRST_WHEEL_GRENADE,
	LOADOUT_POSITION_GRENADE0 = LOADOUT_POSITION_FIRST_WHEEL_GRENADE,
	LOADOUT_POSITION_GRENADE1,
	LOADOUT_POSITION_GRENADE2,
	LOADOUT_POSITION_GRENADE3,
	LOADOUT_POSITION_GRENADE4,
	LOADOUT_POSITION_GRENADE5,
	LOADOUT_POSITION_LAST_WHEEL_GRENADE = LOADOUT_POSITION_GRENADE5,

	// Equipment slots not yet saved in db
	LOADOUT_POSITION_EQUIPMENT0, 
	LOADOUT_POSITION_EQUIPMENT1,
	LOADOUT_POSITION_EQUIPMENT2,
	LOADOUT_POSITION_EQUIPMENT3,
	LOADOUT_POSITION_EQUIPMENT4,
	LOADOUT_POSITION_EQUIPMENT5,
	LOADOUT_POSITION_FIRST_WHEEL_EQUIPMENT = LOADOUT_POSITION_EQUIPMENT0,
	LOADOUT_POSITION_LAST_WHEEL_EQUIPMENT = LOADOUT_POSITION_EQUIPMENT5,

	// Only glove slot saved in db
	LOADOUT_POSITION_CLOTHING_APPEARANCE,
	LOADOUT_POSITION_CLOTHING_TORSO,								
	LOADOUT_POSITION_CLOTHING_LOWERBODY,								
	LOADOUT_POSITION_CLOTHING_HANDS = 41,		// ALREADY SHIPPED, idx must == 41
	LOADOUT_POSITION_CLOTHING_HAT,						
	LOADOUT_POSITION_CLOTHING_FACEMASK,
	LOADOUT_POSITION_CLOTHING_EYEWEAR,									
	LOADOUT_POSITION_CLOTHING_CUSTOMHEAD,							
	LOADOUT_POSITION_CLOTHING_CUSTOMPLAYER,								
	LOADOUT_POSITION_MISC0,								
	LOADOUT_POSITION_MISC1,									
	LOADOUT_POSITION_MISC2,									
	LOADOUT_POSITION_MISC3,									
	LOADOUT_POSITION_MISC4,									
	LOADOUT_POSITION_MISC5,								
	LOADOUT_POSITION_MISC6,								
	LOADOUT_POSITION_FIRST_COSMETIC = LOADOUT_POSITION_CLOTHING_APPEARANCE,
	LOADOUT_POSITION_LAST_COSMETIC = LOADOUT_POSITION_MISC6,

	// ^^^ Warning: CLOTHING COSEMTIC amount of enum entries doesn't match released enum, so must be shuffled before shipping to match public SQL!
	LOADOUT_POSITION_MUSICKIT	= 54,
	LOADOUT_POSITION_FLAIR0		= 55,
	LOADOUT_POSITION_SPRAY0		= 56,
	LOADOUT_POSITION_FIRST_ALL_CHARACTER = LOADOUT_POSITION_MUSICKIT,
	LOADOUT_POSITION_LAST_ALL_CHARACTER = LOADOUT_POSITION_SPRAY0,

	LOADOUT_POSITION_COUNT,
};

#define LOADOUT_POSITION_NUM_AUTOBUY_WEAPONS ( ( LOADOUT_POSITION_LAST_AUTO_BUY_WEAPON - LOADOUT_POSITION_FIRST_AUTO_BUY_WEAPON) + 1 )
#define LOADOUT_POSITION_NUM_COSMETICS ( ( LOADOUT_POSITION_LAST_COSMETIC - LOADOUT_POSITION_FIRST_COSMETIC ) + 1 )
#define LOADOUT_POSITION_NUM_WHEEL_WEAPONS ( ( LOADOUT_POSITION_LAST_WHEEL_WEAPON - LOADOUT_POSITION_FIRST_WHEEL_WEAPON ) + 1 )
#define LOADOUT_POSITION_NUM_WHEEL_GRENADE ( ( LOADOUT_POSITION_LAST_WHEEL_GRENADE - LOADOUT_POSITION_FIRST_WHEEL_GRENADE ) + 1 )
#define LOADOUT_POSITION_NUM_WHEEL_EQUIPMENT ( ( LOADOUT_POSITION_LAST_WHEEL_EQUIPMENT - LOADOUT_POSITION_FIRST_WHEEL_EQUIPMENT ) + 1 )
#define LOADOUT_POSITION_NUM_ALL_CHARACTER ( ( LOADOUT_POSITION_LAST_ALL_CHARACTER - LOADOUT_POSITION_FIRST_ALL_CHARACTER ) + 1 )

#define LOADOUT_NUM_PANELS ( LOADOUT_POSITION_NUM_AUTOBUY_WEAPONS + LOADOUT_POSITION_NUM_COSMETICS + LOADOUT_POSITION_NUM_WHEEL_WEAPONS + LOADOUT_POSITION_NUM_ALL_CHARACTER )

// We use this to determine the maximum number of wearable instances we'll send from the server down to connected clients.
// This hardcoded because of the way RecvPropUtlVector works we can't easily change this 
// without doing some kludgy work and breaking network/demo compatibility.
// Make sure this is up to date before shipping with enough overhead and don't change it without serious consideration.
#define	LOADOUT_MAX_WEARABLES_COUNT ( 1 /*LOADOUT_POSITION_NUM_COSMETICS*/ )


inline bool IsWearableSlot( int iSlot ) 
{
	return ( iSlot >= LOADOUT_POSITION_FIRST_COSMETIC && iSlot <= LOADOUT_POSITION_LAST_COSMETIC );
}

inline bool SlotContainsBaseItems( int iSlot )
{
	// Primary wheel 2 has 6 base item slots... all the rest have 5 and one empty
	if ( iSlot < LOADOUT_POSITION_MELEE )
	{
		return false;
	}

	if ( iSlot >= LOADOUT_POSITION_FIRST_WHEEL_WEAPON && iSlot <= LOADOUT_POSITION_LAST_WHEEL_WEAPON )
	{
		switch( iSlot )
		{
		case LOADOUT_POSITION_SECONDARY5:
		case LOADOUT_POSITION_SMG5:
		case LOADOUT_POSITION_HEAVY5:
			return false;
		}

		return true;
	}

	if ( iSlot >= LOADOUT_POSITION_FIRST_WHEEL_GRENADE && iSlot <= LOADOUT_POSITION_LAST_WHEEL_GRENADE )
	{
// 		switch( iSlot )
// 		{
// 		case LOADOUT_POSITION_GRENADE5:
// 			return false;
// 		}

		return true;
	}

	if ( iSlot >= LOADOUT_POSITION_FIRST_WHEEL_EQUIPMENT && iSlot <= LOADOUT_POSITION_LAST_WHEEL_EQUIPMENT )
	{
		switch( iSlot )
		{
		case LOADOUT_POSITION_EQUIPMENT0:
		case LOADOUT_POSITION_EQUIPMENT1:
		case LOADOUT_POSITION_EQUIPMENT3:
		case LOADOUT_POSITION_EQUIPMENT4:
		case LOADOUT_POSITION_EQUIPMENT5:
			return false;
		}

		return true;
	}

	if ( iSlot >= LOADOUT_POSITION_FIRST_COSMETIC && iSlot <= LOADOUT_POSITION_LAST_COSMETIC )
	{
		switch( iSlot )
		{
		case LOADOUT_POSITION_CLOTHING_APPEARANCE:
		case LOADOUT_POSITION_CLOTHING_TORSO:
		case LOADOUT_POSITION_CLOTHING_LOWERBODY:
		case LOADOUT_POSITION_CLOTHING_HANDS:
		case LOADOUT_POSITION_CLOTHING_HAT:
		case LOADOUT_POSITION_CLOTHING_FACEMASK:
		case LOADOUT_POSITION_CLOTHING_EYEWEAR:
		case LOADOUT_POSITION_CLOTHING_CUSTOMHEAD:
		case LOADOUT_POSITION_CLOTHING_CUSTOMPLAYER:
			return true;
		}

		return false;
	}

	if ( iSlot == LOADOUT_POSITION_MUSICKIT )
		return true;

	if ( iSlot >= LOADOUT_POSITION_FIRST_ALL_CHARACTER && iSlot <= LOADOUT_POSITION_LAST_ALL_CHARACTER )
	{
		return false;
	}

	return true;
}

// The total number of loadouts to track for each player.
#define LOADOUT_COUNT					(2+2)	// these are the number of skins (2 valid + 2 invalid)

//-----------------------------------------------------------------------------
// The maximum number of presets per class - CS doesn't actually use all 10
// right now, though.
//-----------------------------------------------------------------------------
#define MAX_LOADOUT_PRESET_COUNT	10


enum xp_category_t
{
	kXPCategory_None = 0,
	kXPCategory_ContributionScore = 1,		// Server can send ContributionScore (mutually exclusive with CompetitiveRoundWins)
	kXPCategory_CompetitiveRoundWins = 2,	// Server can send CompetitiveRoundWins (mutually exclusive with ContributionScore)
	kXPCategory_BonusBoost = 3,				// GC rewards this weekly boost
	kXPCategory_Overwatch = 4,				// GC rewards Overwatch XP in post SQL-transaction
	kXPCategory_OverwatchBonusBoost = 5,	// GC rewards Overwatch Bonus XP in post SQL-transaction
	kXPCategory_QuestReward = 6,			// Server sends evaluated quest points, and GC rewards Quest XP from coin progress
	kXPCategory_QuestBonus = 7,				// Server sends evaluated quest bonus points, and GC rewards Quest XP from coin progress
	kXPCategory_QuestEvent = 8,				// Server sends quest event XP, GC relays it to client
	// kXPCategory_Headshots, etc
	
	// GC can also use reduced XP reasons -
	kXPCategory_ContributionScoreReduced = 51,		// ContributionScore remapped by GC for client display
	kXPCategory_CompetitiveRoundWinsReduced = 52,	// CompetitiveRoundWins remapped by GC for client display
	kXPCategory_OverwatchReduced = 54,				// Overwatch XP reason remapped by GC for client display
	kXPCategory_QuestEventReduced = 58,				// QuestEvent XP reason remapped by GC for client display

	// GC can also use introductory XP reasons -
	kXPCategory_ContributionScoreIntroductory = 81,		// ContributionScore remapped by GC for client display
	kXPCategory_CompetitiveRoundWinsIntroductory = 82,	// CompetitiveRoundWins remapped by GC for client display
	kXPCategory_QuestEventIntroductory = 88,				// QuestEvent XP reason remapped by GC for client display

	// NEW ENTRIES MUST BE ADDED AT THE BOTTOM
};

const uint32 k_nCSGOXpPerLevel = 5000;
const uint32 k_nCSGOXpMaxLevel = 40;

// Prime status
const uint32 k_nCSGOMinLevelForPrimeStatus = 21;

inline uint32 CSGOXpPointsToLevel( uint32 unExperiencePoints ) { return Min( k_nCSGOXpMaxLevel, 1 + ( unExperiencePoints / k_nCSGOXpPerLevel ) ); }
inline uint32 CSGOXpPointsToXpPoints( uint32 unExperiencePoints ) { return unExperiencePoints % k_nCSGOXpPerLevel; }
inline uint32 CSGOXpPointsToXpWirePoints( uint32 unExperiencePoints )
{
	COMPILE_TIME_ASSERT( k_nCSGOXpPerLevel < 0xFFFF );
	Assert( unExperiencePoints <= 0xFFFF );
	return ( ( unExperiencePoints ) & 0xFFFF ) | ( ( k_nCSGOXpPerLevel & 0xFFFF ) << 16 );
}
inline uint32 CSGOXpPointsFromXpWirePoints( uint32 unXpWirePoints )
{
	COMPILE_TIME_ASSERT( k_nCSGOXpPerLevel < 0xFFFF );
	uint32 const unWireBase = ( unXpWirePoints >> 16 ) & 0xFFFF;
	if ( !unWireBase || ( unWireBase == k_nCSGOXpPerLevel ) )
		return unXpWirePoints & 0xFFFF;
	else
		return uint32( ( double( unXpWirePoints & 0xFFFF ) / double( unWireBase ) ) * double( k_nCSGOXpPerLevel ) );
}

// STORED IN DATABASE! Do not renumber 
enum 
{
	kCSXpBonusFlags_EarnedXpThisPeriod	= 1 << 0,
	kCSXpBonusFlags_FirstReward			= 1 << 1,
	kCSXpBonusFlags_Msg_YourReportGotConvicted	= 1 << 2,
	kCSXpBonusFlags_Msg_YouPartiedWithCheaters	= 1 << 3,
	kCSXpBonusFlags_PrestigeEarned	= 1 << 4,
	// ===$CHINAGOVERNMENTCERT$===
	kCSXpBonusFlags_ChinaGovernmentCert	= 1 << 5,
	// ===$CHINAGOVERNMENTCERT$===

	// Client-facing bits not backed by SQL
	kCSXpBonusFlags_OverwatchBonus		= 1 << 28,
	kCSXpBonusFlags_BonusBoostConsumed	= 1 << 29,
	kCSXpBonusFlags_ReducedGain			= 1 << 30,

	kCSXpBonusFlagsMask_SQLBacked_TimeBased = ( kCSXpBonusFlags_EarnedXpThisPeriod | kCSXpBonusFlags_FirstReward ),
	kCSXpBonusFlagsMask_SQLBacked_Notifications = ( kCSXpBonusFlags_Msg_YourReportGotConvicted | kCSXpBonusFlags_Msg_YouPartiedWithCheaters ),
	kCSXpBonusFlagsMask_SQLBacked_Permanent = ( kCSXpBonusFlagsMask_SQLBacked_Notifications | kCSXpBonusFlags_PrestigeEarned
		| kCSXpBonusFlags_ChinaGovernmentCert // ===$CHINAGOVERNMENTCERT$===
	),
	kCSXpBonusFlagsMask_SQLBacked = ( kCSXpBonusFlagsMask_SQLBacked_TimeBased | kCSXpBonusFlagsMask_SQLBacked_Permanent ),
	kCSXpBonusFlagsMask_Client_Permanent = ( kCSXpBonusFlags_OverwatchBonus ),
	kCSXpBonusFlagsMask_Client_TimeBased = ( kCSXpBonusFlags_BonusBoostConsumed | kCSXpBonusFlags_ReducedGain ),
};

// Used in scoreboard to show other players the reason for the drop. Standard weekly drops are of type 'None'. 
enum
{
	kCSGODropReason_None	= 0,
	kCSGODropReason_Quest	= 1,
	kCSGODropReason_LevelUp	= 2,
};


#endif // CSTRIKE15_ITEM_CONSTANTS_H
