//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			Game Coordinator for CS:GO.  Note that we never use types
//			with undefined length (like int).  Always use an explicit type 
//			(like int32).
//
//=============================================================================

#ifndef CSTRIKE15_GCCONSTANTS_H
#define CSTRIKE15_GCCONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

//=============================================================================

enum EMsgGCCStrike15_v2_ClientLogonFatalError
{
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_None = 0,		// Should not be used (default = no error and logon can succeed)
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_MustUsePWLauncher = 1,	// Client must use PW launcher
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_MustUseSteamLauncher = 2,	// Client must use Steam launcher
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_AccountLinkPWMissing  = 3,	// Client hasn't linked their Steam and PW accounts
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_CustomMessageBase = 1000,	// Custom message has been provided by the 3rd party server (Perfect World)
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingState_t
{
	k_EMsgGCCStrike15_v2_MatchmakingState_None = 0,				// Client is not in matchmaking pool
	k_EMsgGCCStrike15_v2_MatchmakingState_Joined = 1,			// Client joined matchmaking pool
	k_EMsgGCCStrike15_v2_MatchmakingState_Searching = 2,		// Client is searching in matchmaking pool for some time
	k_EMsgGCCStrike15_v2_MatchmakingState_BackToSearching = 3,	// Client has a match which failed to get confirmed by other players
	k_EMsgGCCStrike15_v2_MatchmakingState_MatchConfirmed = 4,	// Client's match is confirmed and client is removed from matchmaking pool
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingMatchOutcome_t
{
	k_EMsgGCCStrike15_v2_MatchmakingMatchOutcome_ResultMask = 0x3,			// Typical match result ( 0 = tie, 1 = first team wins, 2 = second team wins, 3 = legacy incomplete )
	k_EMsgGCCStrike15_v2_MatchmakingMatchOutcome_Flag_NetworkEvent = 0x4,	// Network event occurred during the match
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingGameComposition_t
{
	k_EMsgGCCStrike15_v2_MatchmakingGameComposition_bits_Game = 7,
	k_EMsgGCCStrike15_v2_MatchmakingGameComposition_bits_MapGroup = 24,
};

enum EMsgGCCStrike15_v2_MatchmakingGame_t // & 0xF (values from 1 to 15)
{
	// 1 .. 2 .. 3 are available
	k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace			= 4,
	k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition			= 5,
	k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch			= 6,
	k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual		= 7,
	k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive	= 8, // Used since October 2012
	k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative		= 9,
	k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2		= 10, // Used since April 2017
	k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5		= 11, // Used since April 2017
	k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish			= 12, // Used since April 2017
	// 11 .. 15 are available
};

enum EPlayerRankPipsTypeID_t // STORED IN SQL!
{
	k_EPlayerRankPipsTypeID_Undefined = 0, // should never be used
	k_EPlayerRankPipsTypeID_ScrimComp2v2_2017 = 1, // Spring 2017 comp 2v2
	k_EPlayerRankPipsTypeID_ScrimComp5v5_2017 = 2, // Spring 2017 comp 5v5 weapons expert
};

enum EMsgGCCStrike15_v2_SeasonTime_t
{
	k_EMsgGCCStrike15_v2_SeasonTime_2013Autumn = 1392822576,	// Operation Bravo ended
	k_EMsgGCCStrike15_v2_SeasonTime_2014Winter = 1402958328,	// Operation Phoenix ended
	k_EMsgGCCStrike15_v2_SeasonTime_2014Summer = 1412899200,	// Operation Breakout ended
	k_EMsgGCCStrike15_v2_SeasonTime_2015Spring = 1430179200,	// Operation Vanguard ended
	k_EMsgGCCStrike15_v2_SeasonTime_2015Autumn = 1449089753,	// Operation Bloodhound ended
	k_EMsgGCCStrike15_v2_SeasonTime_2016Summer = 1472688000,    // Operation Wildfire ended
};


enum EMsgGCCStrike15_v2_MatchmakingMapGroup_t // combines with Game_t above, 24 bits available (up to 1<<23)
{
	// NOTE: Changing  names/values in this enum will break old match info records, which have their
	//       map group encoded using this enum, and mapped to map name.  For details, see
	//       MatchmakingGameTypeMapToString()

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust		= ( 1 << 0 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2		= ( 1 << 1 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_train		= ( 1 << 2 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_aztec		= ( 1 << 3 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_inferno		= ( 1 << 4 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_nuke		= ( 1 << 5 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_vertigo		= ( 1 << 6 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mirage		= ( 1 << 7 ), // (0x080)

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_office		= ( 1 << 8 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_italy		= ( 1 << 9 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_assault		= ( 1 << 10 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_militia		= ( 1 << 11 ), // (0x800)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cache		= ( 1 << 12 ),	// Bravo, Phoenix => free

	// 13-19 operation maps

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	= ( 1 << 20 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		= ( 1 << 21 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_canals		= ( 1 << 22 ),

	// 23 free, should be debugged extensively before using as it makes numbers negative (when combined with game type for C++ and SQL)
	// k_EMsgGCCStrike15_v2_MatchmakingMapGroup_questionable	= ( 1 << 23 ),


	// End of Operation Bravo: 1392822576
	// Your time zone: 2/19/2014 7:09:36 AM GMT-8

	// End of Operation Phoenix: 1402958328
	// Your time zone: 6/16/2014 3:38:48 PM GMT-7

	// End of Operation Breakout: 1412899200
	// Your time zone: 10/9/2014 5:00:00 PM GMT-7

	// End of Operation Vanguard: 1430179200
	// Your time zone: 4/27/2015, 5:00:00 PM GMT-7

	// End of Operation Bloodhound: 1449089753
	// Your time zone: 4/27/2015, 5:00:00 PM GMT-7

	// End of Operation Wildfire: 1472688000 (significantly before this, actually)
	//                 9/1/2016, 0:00:00 AM UTC

// defined in Valve main group--
//	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cache		= ( 1 << 12 ),	// Bravo, Phoenix => free

	// Operation maps (7)

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map01		= ( 1 << 13 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_gwalior		= ( 1 << 13 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_motel		= ( 1 << 13 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_rush		= ( 1 << 13 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_season		= ( 1 << 13 ),	// Vanguard, Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_cruise		= ( 1 << 13 ),	// Wildfire

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map02		= ( 1 << 14 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_ali			= ( 1 << 14 ),	// Bravo, Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_blackgold	= ( 1 << 14 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_marquis		= ( 1 << 14 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_log			= ( 1 << 14 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_coast		= ( 1 << 14 ),	// Wildfire

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map03		= ( 1 << 15 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_ruins		= ( 1 << 15 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_thunder		= ( 1 << 15 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mist		= ( 1 << 15 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_facade		= ( 1 << 15 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_rails		= ( 1 << 15 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_empire		= ( 1 << 15 ),	// Wildfire

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map04		= ( 1 << 16 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_chinatown	= ( 1 << 16 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_favela		= ( 1 << 16 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_insertion	= ( 1 << 16 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_backalley	= ( 1 << 16 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_resort		= ( 1 << 16 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mikla		= ( 1 << 16 ),	// Wildfire

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map05		= ( 1 << 17 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_seaside		= ( 1 << 17 ),	// Payback, Bravo, Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overgrown	= ( 1 << 17 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_workout		= ( 1 << 17 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_zoo			= ( 1 << 17 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_royal		= ( 1 << 17 ),	// Wildfire

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map06		= ( 1 << 18 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_siege		= ( 1 << 18 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_downtown	= ( 1 << 18 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_castle		= ( 1 << 18 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bazaar		= ( 1 << 18 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_santorini	= ( 1 << 18 ),	// Wildfire

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map07		= ( 1 << 19 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_agency		= ( 1 << 19 ),	// Bravo, Phoenix, Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_tulip		= ( 1 << 19 ),	// Wildfire

// defined in Valve main group--
// 	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	= ( 1 << 20 ),
// 	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		= ( 1 << 21 ),
//	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_canals		= ( 1 << 22 ),

	// 
	// NO MORE BITS AVAILABLE HERE UNFORTUNATELY
	// ALL ACTIVE MAPGROUPS MUST FIT IN 0xFFFFFF (24 bits, 1<<0 to 1<<23)
	// 1<<23 should be debugged extensively before using as it makes numbers negative (when combined with game type for C++ and SQL)
	//

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_tournament_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_inferno		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_nuke		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mirage		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_train		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cache		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_operation_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map01		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map02		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map03		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map04		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map05		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map06		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map07		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_reserves_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_aztec		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_vertigo		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_canals		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_office		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_italy		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_assault		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_militia		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_all_valid =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_tournament_maps|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_operation_maps	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_reserves_maps	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage_maps	|
		0,

	// --- Begin special mapgroups for non-competitive game modes
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_shoots		= ( 1 << 0 ),	// AR
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bank		= ( 1 << 0 ),	//      DEMO + 2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_baggage		= ( 1 << 1 ),	// AR
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shorttrain	= ( 1 << 1 ),	//      DEMO + 2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_monastery	= ( 1 << 2 ),	// AR
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_sugarcane	= ( 1 << 2 ),	//      DEMO + 2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		= ( 1 << 3 ),	// AR + DEMO + 2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		= ( 1 << 4 ),	// AR + DEMO + 2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	= ( 1 << 5 ),	// AR + DEMO + 2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortdust	= ( 1 << 6 ),	//      DEMO + 2v2
	// --- combination of special mapgroups for non-competitive game modes
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_AR				=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_shoots		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_baggage		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_monastery	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	|
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_DEMO			=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bank		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shorttrain	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_sugarcane	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortdust |
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ScrimComp2v2			=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bank		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shorttrain	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortdust |
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_active			= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_tournament_maps,	// Active group
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_dust247		= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2,		// Dust 2 (24x7) group
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_reserves		= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_reserves_maps,	// Reserves group
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage		= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage_maps,	// Hostage group
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_op07		= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_operation_maps,	// Operation 7 group
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish		= ( 1 << 22 ) - 1, // = 0x3fffff.  Up to 22 simultaneous skirmish modes are supported
	// --- end of special mapgroups for non-competitive game modes
};

enum EMsgGCCStrike15_v2_MatchmakingMap_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCCStrike15_v2_MatchmakingMap_undefined			= 0,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_dust				= 1,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_dust2			= 2,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_train			= 3,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_aztec			= 4,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_inferno			= 5,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_nuke				= 6,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_vertigo			= 7,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_office			= 8,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_italy			= 9,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_baggage			= 10,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_baloney			= 11,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_monastery		= 12,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_shoots			= 13,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_bank				= 14,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_glass			= 15,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_lake				= 16,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_safehouse		= 17,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_shorttrain		= 18,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_stmarc			= 19,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_sugarcane		= 20,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_assault			= 21,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_militia			= 22,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_mirage			= 23,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_cache			= 24,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_gwalior			= 25,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_ali				= 26,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_ruins			= 27,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_chinatown		= 28,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_seaside			= 29,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_siege			= 30,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_agency			= 31,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_overpass			= 32,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_cbble			= 33,

	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_motel			= 34,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_downtown			= 35,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_thunder			= 36,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_favela			= 37,

	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_rush				= 38,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_mist				= 39,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_castle			= 40,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_overgrown		= 41,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_insertion		= 42,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_blackgold		= 43,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_season			= 44,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_marquis			= 45,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_facade			= 46,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_backalley		= 47,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_workout			= 48,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_bazaar			= 49,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_shortdust		= 50,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_rails			= 51,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_resort			= 52,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_zoo				= 53,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_log				= 54,

	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_crashsite		= 55,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_lake				= 56,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_bank				= 57,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_cbble			= 58,

	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_cruise			= 59,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_coast			= 60,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_empire			= 61,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_mikla			= 62,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_royal			= 63,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_santorini		= 64,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_tulip			= 65,

	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_sugarcane		= 66,
	k_EMsgGCCStrike15_v2_MatchmakingMap_coop_cementplant	= 67,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_canals			= 68,
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};

//=============================================================================

enum EMsgGCCStrike15_v2_SessionNeed_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//
	// They represent client state to help GC better schedule logon surges
	//

	k_EMsgGCCStrike15_v2_SessionNeed_Default = 0,
	k_EMsgGCCStrike15_v2_SessionNeed_OnServer = 1,
	k_EMsgGCCStrike15_v2_SessionNeed_FindGame = 2,
	k_EMsgGCCStrike15_v2_SessionNeed_PartyLobby = 3,
	k_EMsgGCCStrike15_v2_SessionNeed_Overwatch = 4,

	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//
};

//=============================================================================

enum EMsgGCCStrike15_v2_AccountActivity_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//
	// (0-0xF for compact GC representation)
	//

	k_EMsgGCCStrike15_v2_AccountActivity_None				= 0,
	k_EMsgGCCStrike15_v2_AccountActivity_Playing			= 1,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecConnected		= 2,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecGOTV			= 3,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecOverwatch		= 4,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecTwitch			= 5,
	k_EMsgGCCStrike15_v2_AccountActivity_count				= 6,

	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//

	k_EMsgGCCStrike15_v2_AccountActivity_RatelimitSeconds	= 180, // activity messages are ratelimited
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingKickBanReason_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_VotedOff	= 1,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_TKLimit	= 2,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_TKSpawn	= 3,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_DisconnectedTooLong	= 4,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_Abandoned	= 5,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_THLimit	= 6,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_THSpawn	= 7,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan = 8,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_KickedTooMuch = 9,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating = 10,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForBehavior = 11,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_Abandoned_Grace = 12,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_DisconnectedTooLong_Grace = 13,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification = 14,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_NoUserSession = 15,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_FailedToConnect = 16,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_KickAbuse = 17,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_SkillGroupCalibration = 18,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation = 19,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation_Repeated = 20,

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};

inline bool EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsGlobal( uint32 eReason )
{
	switch ( eReason )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForBehavior:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation_Repeated:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_NoUserSession:
		return true;
	default:
		return false;
	}
}

inline bool EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsPermanent( uint32 eReason )
{
	switch ( eReason )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification:
		return true;
	default:
		return false;
	}
}

inline bool EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsGreen( uint32 eReason )
{
	switch ( eReason )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_Abandoned_Grace:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_DisconnectedTooLong_Grace:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_SkillGroupCalibration:
		return true;
	default:
		return false;
	}
}

//=============================================================================

enum EMMV2OverwatchCasesVerdict_t
{
	// CSGO V2 Overwatch case verdict field, stored in SQL
	k_EMMV2OverwatchCasesVerdict_Pending = 0,
	k_EMMV2OverwatchCasesVerdict_Dismissed = 1,
	k_EMMV2OverwatchCasesVerdict_ConvictedForCheating = 2,
	k_EMMV2OverwatchCasesVerdict_ConvictedForBehavior = 3,
};

enum EMMV2OverwatchCasesUpdateReason_t
{
	// CSGO V2 Overwatch case update request reason, used for communication between client and GC
	k_EMMV2OverwatchCasesUpdateReason_Poll = 0,		// Client is polling for an overwatch case
	k_EMMV2OverwatchCasesUpdateReason_Assign = 1,	// Client is eager to get a case assigned and work on it
	k_EMMV2OverwatchCasesUpdateReason_Downloading = 2,	// Client is downloading the case files
	k_EMMV2OverwatchCasesUpdateReason_Verdict = 3,	// Client is willing to cast a verdict on a previously assigned case
};

enum EMMV2OverwatchCasesStatus_t
{
	// CSGO V2 Overwatch case status field, stored in SQL
	k_EMMV2OverwatchCasesStatus_Default = 0,
	k_EMMV2OverwatchCasesStatus_Ready = 1,
	k_EMMV2OverwatchCasesStatus_ErrorDownloading = 2,
	k_EMMV2OverwatchCasesStatus_ErrorExtracting = 3,
};

enum EMMV2OverwatchCasesType_t
{
	// CSGO V2 Overwatch case type field, stored in SQL
	k_EMMV2OverwatchCasesType_Reports = 0,
	k_EMMV2OverwatchCasesType_Placebo = 1,
	k_EMMV2OverwatchCasesType_VACSuspicion = 2,
	k_EMMV2OverwatchCasesType_Manual = 3,
	k_EMMV2OverwatchCasesType_MLSuspicion = 4,
	k_EMMV2OverwatchCasesType_Max = 5,
};

//=============================================================================

inline uint32 MatchmakingGameTypeCompose( EMsgGCCStrike15_v2_MatchmakingGame_t eGame, EMsgGCCStrike15_v2_MatchmakingMapGroup_t eMapGroup )
{
	return ( ( uint32( eGame ) & 0xF ) << 0 ) | ( ( uint32( eMapGroup ) & 0xFFFFFF ) << 8 );
}

inline EMsgGCCStrike15_v2_MatchmakingGame_t MatchmakingGameTypeToGame( uint32 uiGameType )
{
	return ( EMsgGCCStrike15_v2_MatchmakingGame_t ) ( ( uiGameType >> 0 ) & 0xF );
}

inline EMsgGCCStrike15_v2_MatchmakingMapGroup_t MatchmakingGameTypeToMapGroup( uint32 uiGameType )
{
	return ( EMsgGCCStrike15_v2_MatchmakingMapGroup_t ) ( ( uiGameType >> 8 ) & 0xFFFFFF );
}

inline EPlayerRankPipsTypeID_t MatchmakingGameTypeToPipsTypeID( uint32 uiGameType )
{
	switch ( MatchmakingGameTypeToGame( uiGameType ) )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2:
		return k_EPlayerRankPipsTypeID_ScrimComp2v2_2017;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5:
		return k_EPlayerRankPipsTypeID_ScrimComp5v5_2017;
	default:
		return k_EPlayerRankPipsTypeID_Undefined;
	}
}

inline bool MatchmakingGameTypeGameIsQueued( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5:
		return true;
	default:
		return false;
	}
}

inline bool MatchmakingGameTypeGameIsQueuedWithMostStats( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5:
		return true;
	default:
		return false;
	}
}

inline bool MatchmakingGameTypeGameIsQueuedWithFullMatchStats( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive:
		return true;
	default:
		return false;
	}
}

inline bool MatchmakingGameTypeGameIsSingleMapGroup( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish:
		return true;
	default:
		return false;
	}
}


inline EMsgGCCStrike15_v2_MatchmakingMapGroup_t MatchmakingGameTypeMapGroupExtendToLargeGroup( EMsgGCCStrike15_v2_MatchmakingGame_t eGame, EMsgGCCStrike15_v2_MatchmakingMapGroup_t eGroup )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace:
		return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_AR;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition:
		return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_DEMO;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual:
#define MAPGROUPENUM( mgname ) if ( k_EMsgGCCStrike15_v2_MatchmakingMapGroup_##mgname & eGroup ) return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_##mgname;
	/** Removed for partner depot **/
#undef MAPGROUPENUM
		return eGroup;
	default:
		return eGroup;
	}
}

inline uint32 MatchmakingGameTypeGameMaxPlayers( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch: return 16;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual: return 20;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative: return 2;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2: return 4;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish: return 16; // $$$REI TODO Decide # of players in skirmish.  Right now using DM as base.
	default: return 10;
	}
}

inline char const * MatchmakingGameTypeMapToString( EMsgGCCStrike15_v2_MatchmakingMapGroup_t eMapGroup, uint64 uiMatchID )
{
	char const *szMap = NULL;
	/** Removed for partner depot **/
	return szMap;
}

//=============================================================================

enum EMsgGCCStrike15_v2_WatchInfoConstants_t
{
	k_EMsgGCCStrike15_v2_WatchInfoConstants_MaxAccountsBatchSize = 50,		// How many accounts can be requested in a batch
	k_EMsgGCCStrike15_v2_WatchInfoConstants_MaxAccountsBatchRate = 5,		// 5 requests per minute are allowed
};

//=============================================================================

enum EMsgGCCStrike15_v2_GC2ClientMsgType
{
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_Unconnected = 0,					// Client is Unconnected
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_Unauthorized = 1,					// Client is Unauthorized
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_Unrecognized = 2,					// Unrecognized request
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_BadPayload = 3,					// Request was recognized, but payload was bad
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_ExecutionError = 4,				// Request was not executed
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_PrintTextWarning = 5,				// Response is warning text to be displayed to client
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_PrintTextInfo = 6,				// Response is informational text to be displayed to client
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_WriteFile = 7,					// Response is potentially binary payload to be written to response file
};

//=============================================================================

enum EMsgGCCStrike15_v2_GC2ClientNoteType
{
	k_EMsgGCCStrike15_v2_GC2ClientNoteType_None = 0,						// Nothing
	k_EMsgGCCStrike15_v2_GC2ClientNoteType_ClusterLoadHigh = 1,				// Datacenter has high load
	k_EMsgGCCStrike15_v2_GC2ClientNoteType_ClusterOffline = 2,				// Datacenter is offline
};

//=============================================================================


//=============================================================================

enum EMsgGCAccountPrivacySettingsType_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCAccountPrivacySettingsType_PlayerProfile = 1,					// Player profile including competitive information and commendations

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};
enum EMsgGCAccountPrivacySettingsValue_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCAccountPrivacySettingsValue_Default = 1,					// Setting should be reset to default for the player
	k_EMsgGCAccountPrivacySettingsValue_Disabled = 2,					// Setting should be disabled
	k_EMsgGCAccountPrivacySettingsValue_Enabled = 3,					// Setting should be enabled

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};
inline bool EMsgGCAccountPrivacySettingsType_IsExposedToClient( EMsgGCAccountPrivacySettingsType_t val )
{
	return ( val == k_EMsgGCAccountPrivacySettingsType_PlayerProfile );
}

//=============================================================================

enum EMsgGCAccountPrivacyRequestLevel_t
{
	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//

	k_EMsgGCAccountPrivacySettingsValue_Public_All =		0,					// Requesting data that is already easily available to everybody (e.g. persona name)
	k_EMsgGCAccountPrivacySettingsValue_Public_Shared =		0x10,				// Setting can be shared with public
	k_EMsgGCAccountPrivacySettingsValue_Friends_Only =		0x20,				// Setting shared with friends
	k_EMsgGCAccountPrivacySettingsValue_Private_All =		0x80,				// All information that should already be available to the owner

	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//
};

//=============================================================================

enum EMsgGCVarValueNotificationInfoType_t
{
	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//

	k_EMsgGCVarValueNotificationInfoType_Cmd = 0,					// Data from user cmd
	k_EMsgGCVarValueNotificationInfoType_Divergence = 1,			// Divergence of viewangles
	k_EMsgGCVarValueNotificationInfoType_Inventory = 2,				// Community server misrepresenting inventory or rank

	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//
};

//=============================================================================

enum EMsgGCCStrike15_v2_NqmmRating_t
{
	k_EMsgGCCStrike15_v2_NqmmRating_Version_Current = 1,		// Current version of nqmm rating serialization
};

//=============================================================================

enum EScoreLeaderboardDataEntryTag_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EScoreLeaderboardDataEntryTag_undefined			= 0,
	k_EScoreLeaderboardDataEntryTag_Kills				= 1,
	k_EScoreLeaderboardDataEntryTag_Assists				= 2,
	k_EScoreLeaderboardDataEntryTag_Deaths				= 3,
	k_EScoreLeaderboardDataEntryTag_Points				= 4,
	k_EScoreLeaderboardDataEntryTag_Headshots			= 5,
	k_EScoreLeaderboardDataEntryTag_ShotsFired			= 6,
	k_EScoreLeaderboardDataEntryTag_ShotsOnTarget		= 7,
	k_EScoreLeaderboardDataEntryTag_HpDmgInflicted		= 8,
	k_EScoreLeaderboardDataEntryTag_HpDmgSuffered		= 9,
	k_EScoreLeaderboardDataEntryTag_TimeElapsed			= 10,
	k_EScoreLeaderboardDataEntryTag_TimeRemaining		= 11,
	k_EScoreLeaderboardDataEntryTag_RoundsPlayed		= 12,
	k_EScoreLeaderboardDataEntryTag_BonusPistolOnly		= 13,
	k_EScoreLeaderboardDataEntryTag_BonusHardMode		= 14,
	k_EScoreLeaderboardDataEntryTag_BonusChallenge		= 15,

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};

//=============================================================================

#endif //CSTRIKE15_GCCONSTANTS_H
