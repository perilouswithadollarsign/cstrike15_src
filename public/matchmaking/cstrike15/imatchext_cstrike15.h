//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHEXT_CSTRIKE15_H
#define IMATCHEXT_CSTRIKE15_H

#ifdef _WIN32
#pragma once
#pragma warning( push )
#pragma warning( disable : 4201 )
#endif

#define STORAGE_COUNT_FOR_BITS( aStorageType, numBits ) ( ( (numBits) + 8*sizeof( aStorageType ) - 1 ) / sizeof( aStorageType ) )


// Matchmaking data for CSS1.5
#define MM_AVG_CONST 100.0f

// MatchmakingDataMode
// We keep seperate matchmaking data for a different kinds of types.  Types that
// aren't specific to certain game play rules (like gungame progressive) should
// use the general type.
enum MatchmakingDataType
{
	MMDATA_TYPE_GENERAL = 0,
	MMDATA_TYPE_GGPROGRESSIVE,

	MMDATA_TYPE_COUNT,
};

// MatchmakingDataScope
// There are two kinds of scope for each MatchmakingDataType: Lifetime and Round.
// Lifetime is the user's liifetime matchmaking values serialized to/from the user's
// profile.
// Round is the user's current values accumulated during the current round of game
// play.  At the end of the round, the Round values are aggregated with the lifetime
// values according to the formulas specified in the MatchSystem resource file.
enum MatchmakingDataScope
{
	MMDATA_SCOPE_LIFETIME = 0,
	MMDATA_SCOPE_ROUND,
	
	MMDATA_SCOPE_COUNT,
};

#define MATCHMAKINGDATA_FIELD(name) short name [MMDATA_TYPE_COUNT][MMDATA_SCOPE_COUNT];

// MatchmakingData
// This is the data structure used for matchmaking.  Any fields addsed to this structure
// need to be added to all of the appropriate areas where we calculate averages and 
// serialize this data.
struct MatchmakingData
{
	MATCHMAKINGDATA_FIELD(mContribution);
	MATCHMAKINGDATA_FIELD(mMVPs);
	MATCHMAKINGDATA_FIELD(mKills);
	MATCHMAKINGDATA_FIELD(mDeaths);
	MATCHMAKINGDATA_FIELD(mHeadShots);
	MATCHMAKINGDATA_FIELD(mDamage);
	MATCHMAKINGDATA_FIELD(mShotsFired);
	MATCHMAKINGDATA_FIELD(mShotsHit);
	MATCHMAKINGDATA_FIELD(mDominations);
	MATCHMAKINGDATA_FIELD(mRoundsPlayed);
};

#undef MATCHMAKINGDATA_FIELD


struct PlayerELOBracketInfo_t 
{
	uint8	m_DisplayBracket  : 4;		// Bracket displayed to the user for this game mode / input device combo
	uint8	m_PreviousBracket : 4;		// Bracket we are qualified for based on the last elo change (used in settling code)
	uint8	m_NumGamesInBracket;		// Count of rounds played in the current bracket (used in settling code). 
};

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData1 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData1
{
	uint16 versionNumber;

	struct usrStats_t
	{
	};
	usrStats_t usrStats;

};

//
//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData2 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData2
{
	// CSMedalsAwarded: bool for isAchieved
	// CSMedalsMedalInfo: if awarded, unlocktime; if not awarded, count
	// avoid struct to avoid wasting space with alignment issues
uint16 versionNumber;


};

//
//	WARNING!! WARNING!! WARNING!! WARNING!!
//		This structure TitleData3 should remain
//		intact after we ship otherwise
//		users profiles will be busted.
//		You are allowed to add fields at the end
//		as long as structure size stays under
//		XPROFILE_SETTING_MAX_SIZE = 1000 bytes.
//	WARNING!! WARNING!! WARNING!! WARNING!!
//
struct TitleData3
{
	uint32 version;

	uint16 versionNumber;

	struct ConVarsSystem_t
	{
		uint32 unused_values[10];

//		enum Bits_bitfields_t
//		{
//#define CFG( name ) name,
//#include "xlast_csgo/inc_gameconsole_settings_sys_bits.inc"
//#undef CFG
//			bit_last
//		};
//		uint32 bitfields[ STORAGE_COUNT_FOR_BITS( uint32, bit_last ) ];

		//uint32 unused[10];
	};
	ConVarsSystem_t cvSystem;

	struct ConVarsUser_t
	{

#if defined( _PS3 )

		// Two other sets of button bindings.
		struct MoveBindings_t
		{
#include "xlast_csgo/inc_gameconsole_device_specific_settings_usr.inc"
		};
		MoveBindings_t PSMove;

		struct SharpShooterBindings_t
		{
#include "xlast_csgo/inc_gameconsole_device_specific_settings_usr.inc"
		};
		SharpShooterBindings_t SharpShooter;

#endif // _PS3

#undef CFG

		//uint32 unused_values[10];
//		enum Bits_bitfields_t
//		{
//#define CFG( name ) name,
//#include "xlast_csgo/inc_gameconsole_settings_usr_bits.inc"
//#undef CFG
//			bit_last
//		};
//		uint32 bitfields[ STORAGE_COUNT_FOR_BITS( uint32, bit_last ) ];

		//uint32 unused[10];
	};
	ConVarsUser_t cvUser;
	ConVarsUser_t cvUserSS;

	struct JoystickBindings_t
	{

#if defined( _PS3 )

		// Keyboard bindings.
#include "xlast_csgo/inc_ps3_key_bindings_usr.inc"

		// Two other sets of button bindings.
		struct MoveBindings_t
		{
#include "xlast_csgo/inc_bindings_usr.inc"
		};
		MoveBindings_t PSMove;

		struct SharpShooterBindings_t
		{
#include "xlast_csgo/inc_bindings_usr.inc"
		};
		SharpShooterBindings_t SharpShooter;


#endif

#undef BINDING
#undef ACTION
	};
	JoystickBindings_t JoystickBindings;

	struct GameInstructorData_t
	{
		enum LessonsBits_t
		{
			lessonbits_last_bit_used,
			lessonbits_total = 48 // leave room for total 48 lessons
		};

		union LessonInfo_t
		{
			uint8 u8dummy;
			struct
			{
				uint8 display : 4;
				uint8 success : 4;
			};
		} lessoninfo[ lessonbits_total ];
	};
	GameInstructorData_t gameinstructor;

		//we are using an array so we can pack this info as tight as possible to fit into the 1K block

#if defined( LOCAL_ELO_DATA )
	// array of player skill rankings
	PlayerELORank_t playerRankingsData[ ELOTitleData360::TOTAL_NUM_ELO_RANKS_STORED ];

	PlayerELOBracketInfo_t EloBracketInfo[ ELOTitleData::NUM_GAME_MODES_ELO_RANKED ];
#endif

	struct usrMMData_t
	{
	};
	usrMMData_t usrMMData;
	
};


#ifdef _WIN32
#pragma warning( pop )
#endif
#endif // IMATCHEXT_CSTRIKE15_H
