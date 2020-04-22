//====== Copyright  Valve Corporation, All rights reserved. =================
//
// Achievements (aka Medals) are divided into categories. Progress towards
// completing a category of medals gives the player a higher 'rank'. Player
// rank is sent to the server for display on the scoreboard.
//
// Ranks, Medals and medal-relevant gamestats earned during a round are recorded
// here for the purpose of displaying progress towards ranks to the player.
//
//=============================================================================
#if !defined CS_PLAYER_RANK_MGR_H
#define CS_PLAYER_RANK_MGR_H
#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "matchmaking/imatchframework.h"
#include "GameEventListener.h"
#include "cs_player_rank_shared.h"
#include "cs_gamestats_shared.h"

#if defined ( _GAMECONSOLE ) 
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "cs_client_gamestats.h"
#endif

class KeyValues;
class CBaseAchievement;
class CAchievement_StatGoal;

// Structs for recording rank-relevent events that occur during a round
struct RankIncreasedEvent_t
{	
	RankIncreasedEvent_t( CBaseAchievement* pAch, MedalCategory_t cat )
		:m_pAchievement( pAch ), m_category( cat ) {}

	CBaseAchievement *m_pAchievement;
	MedalCategory_t m_category;
};

struct MedalEarnedEvent_t 
{
	MedalEarnedEvent_t( CBaseAchievement* pAch, MedalCategory_t cat )
		:m_pAchievement( pAch ), m_category( cat ) {}
	CBaseAchievement *m_pAchievement;
	MedalCategory_t m_category;
};

struct MedalStatEvent_t
{
	MedalStatEvent_t( CBaseAchievement* pAch, MedalCategory_t category, CSStatType_t type )
		:m_pAchievement( pAch ), m_category( category ), m_StatType( type ) {}
	CBaseAchievement *m_pAchievement;
	MedalCategory_t m_category;
	CSStatType_t m_StatType;
};

class CPlayerRankManager : public CAutoGameSystem, public CGameEventListener, public IMatchEventsSink
{
public:
	CPlayerRankManager();
	~CPlayerRankManager();

	virtual bool Init();
	virtual void Shutdown();
	virtual void LevelInitPostEntity();

	bool HasBuiltMedalCategories( void ) const;

	// Get rank ups and medals earned during the current round. These lists are reset on round start.
	const CUtlVector<MedalEarnedEvent_t>& GetMedalsEarnedThisRound( void ) const;
	const CUtlVector<RankIncreasedEvent_t>& GetRankIncreasesThisRound( void ) const;

	// Builds a list of progress based achievements (goal > 1) who had their dependant gamestat increased this round.
	void GetMedalStatsEarnedThisRound( CUtlVector<MedalStatEvent_t>& ) const;

	// Number of achievements compeleted by the local player in the given category.
	int CountAchievedInCategory( MedalCategory_t category ) const;
	// Total count of medals in a category
	int GetTotalMedalsInCategory( MedalCategory_t category ) const;
	// Number of medals required to be awarded a given rank
	int GetMinMedalsForRank( MedalCategory_t category, MedalRank_t rank ) const;

	// Blindly sets the rank for the given category
	void SetRankForCategory( MedalCategory_t category, int nRank ) { m_rank[category] = (MedalRank_t)nRank; }
	// Calculates the player's rank for a given category and returns it. Does not set the internal rank variable.
	MedalRank_t CalculateRankForCategory( MedalCategory_t category ) const;

	const char *GetMedalCatagoryName( MedalCategory_t category );
	const char *GetMedalCatagoryRankName( int nRank );

	void NoteEloBracketChanged( int iOldBracket, int iNewBracket );

	// Elo is special: UI display code will explicitly mark when we've shown the
	// 'elo changed' update to the user beacuse we don't want to miss displaying this update.
	int GetEloBracketChange( int &iOutNewEloBracket );
	void ResetRecordedEloBracketChange( void ); 

#if defined( _GAMECONSOLE ) 
	// On consoles, this object holds the elo brackets instead of the GC. 
	int Console_GetEloBracket( ELOGameType_t game_mode, PlayerELOBracketInfo_t *pOutBracket = NULL );
	bool Console_SetEloBracket( ELOGameType_t game_mode, uint8 display_bracket, uint8 prev_bracket, uint8 num_games_in_bracket );
	bool Console_SetEloBracket( ELOGameType_t game_mode, const PlayerELOBracketInfo_t& bracket );
	void ServerRequestBracketInfo( ELOGameType_t game_mode );
#endif 

	// Debug spew.
	void PrintRankProgressThisRound() const;

	// IMatchEventsSink functions
public:
	virtual void OnEvent( KeyValues *pEvent ) OVERRIDE;

private:
	struct MedalCategoryInfo_t
	{
		int m_totalInCategory;
		int m_minCountForRank[MEDAL_RANK_COUNT];
	};

	void FireGameEvent( IGameEvent *event );

	void SendRankDataToServer( void );
	void BuildMedalCategories( void );
	void RecalculateRanks( void );
	void OnRoundStart();
	void OnAchievementEarned( int achievementID );
	void OnSeasonCoinLeveledUp( int iCategory = MEDAL_CATEGORY_NONE, int iRank = MEDAL_RANK_NONE );
	bool CheckCategoryRankLevelUp( int iCategory, int iRank );
	void CheckForStatProgress();

	// Local player's rank data.
	MedalRank_t m_rank[MEDAL_CATEGORY_COUNT];
	// Info about each medal category.
	MedalCategoryInfo_t m_categoryInfo[MEDAL_CATEGORY_COUNT];
	// List of achievements assigned to each category.
	CUtlVector<CBaseAchievement*> m_medalCategoryList[MEDAL_CATEGORY_COUNT];

	// Per-round rank relevant events.
	CUtlVector<RankIncreasedEvent_t> m_RankIncreases;
	CUtlVector<MedalEarnedEvent_t> m_MedalsEarned;

	// List of stat based achievement
	CUtlVector<CAchievement_StatGoal*> m_StatBasedAchievements;

	int m_iEloBracketDelta;
	int m_iNewEloBracket;

	bool m_bMedalCategoriesBuilt;
};

extern CPlayerRankManager g_PlayerRankManager;

const char* GetLocTokenForStatId( const CSStatType_t id );

#endif
