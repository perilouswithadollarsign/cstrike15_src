//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			Inter-Game Coordinator messages.  Note that we never use types
//			with undefined length (like int).  Always use an explicit type 
//			(like int32).
//
//=============================================================================

#ifndef SHARED_GCMESSAGES_H
#define SHARED_GCMESSAGES_H
#ifdef _WIN32
#pragma once
#endif

enum EGCSharedMsg
{
	k_EMsgInterGCBase =								7000,
	k_EMsgInterGCAchievementAwarded =				k_EMsgInterGCBase + 1,
	k_EMsgInterGCAchievementAwardedResponse =		k_EMsgInterGCBase + 2,
	k_EMsgInterGCLoadAchievements =					k_EMsgInterGCBase + 3,
	k_EMsgInterGCLoadAchievementResponse =			k_EMsgInterGCBase + 4,
};

// k_EMsgInterGCAchievementAwarded
struct MsgInterGCAchievementAwarded_t
{
	uint64 m_ulSteamID; // steam ID of the user that earned the achievement
	// Variable data:
	// - name of the achievement
};


// k_EMsgInterGCAchievementAwardedResponse
struct MsgInterGCAchievementAwardedResponse_t
{
	uint64 m_ulSteamID; // steam ID of the user that earned the achievement
	uint64 m_ulItemID; // ID of the item awarded
	bool m_bAppPlayed; // true if the user has played the target game
	// Variable data:
	// - name of the achievement
};


// k_EMsgInterGCLoadAchievements
struct MsgInterGCLoadAchievements_t
{
	uint64 m_ulSteamID; // steam ID of the user that earned the achievement
	uint32 m_cAchievements; // number of achievement names in the variable data
	// Variable data:
	// - name of the achievement
};

// k_EMsgInterGCLoadAchievementsResponse
struct MsgInterGCLoadAchievementsResponse_t
{
	uint64 m_ulSteamID; // steam ID of the user that earned the achievement
	uint32 m_cAchievements; // number of achievement bools in the variable data
	// Variable data:
	// - a bool for the achievement
};


#endif
