//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_STEAMSTATS_H
#define CS_STEAMSTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"
#include "GameEventListener.h"
#include "cs_gamestats_shared.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "steamworks_gamestats_client.h"
#include "gametypes.h"
#include "cs_gamerules.h"

struct PlayerStatData_t
{
	wchar_t*    pStatDisplayName;			// Localized display name of the stat
	int         iStatId;					// CSStatType_t enum value of stat
	int32       iStatValue;                 // Value of the stat
};

enum CSSyncStatValueDirection_t
{
	CSSTAT_WRITE_STAT,
	CSSTAT_READ_STAT
};

enum CSClientCsgoGameEventType_t
{
	// WARNING: must be in sync with the g_CSClientCsgoGameEventTypeNames[] in cpp
	k_CSClientCsgoGameEventType_SprayApplication = 1,
	k_CSClientCsgoGameEventType_ConnectionProblem_Generic = 2,
	k_CSClientCsgoGameEventType_ConnectionProblem_Loss = 3,
	k_CSClientCsgoGameEventType_ConnectionProblem_Choke = 4,
	// WARNING: must be in sync with the g_CSClientCsgoGameEventTypeNames[] in cpp
};
#define CSCLIENTCSGOGAMEEVENTTYPE_AUTODETECT_INT16 (-32768)

//=============================================================================
//
// OGS Gamestats
//
#if !defined( _GAMECONSOLE )
struct SRoundData : public BaseStatData 
{
	explicit SRoundData( const StatsCollection_t *pRoundData ) 
	{
		nRoundTime		= (*pRoundData)[CSSTAT_PLAYTIME];
		nWasKilled		= (*pRoundData)[CSSTAT_DEATHS];
		nIsMVP			= (*pRoundData)[CSSTAT_MVPS];
		nMoneySpent		= (*pRoundData)[CSSTAT_MONEY_SPENT];
		nStartingMoney	= -1;// We'll get this data separately 
		nRoundScore			= 0;
		nRevenges		= (*pRoundData)[CSSTAT_REVENGES];
		nDamageDealt	= (*pRoundData)[CSSTAT_DAMAGE];
		llExperimental	= 0;

		nTeamID = 0;
		nWinningTeamID = 0;

		if ( (*pRoundData)[CSSTAT_T_ROUNDS_WON] )
		{
			nWinningTeamID = TEAM_TERRORIST;
			/*
			if ( (*pRoundData)[CSSTAT_ROUNDS_WON] )
			{
				nTeamID = TEAM_TERRORIST;
			}
			else
			{
				nTeamID = TEAM_CT;
			}
			*/
		}
		else if ( (*pRoundData)[CSSTAT_CT_ROUNDS_WON] )
		{
			nWinningTeamID = TEAM_CT;
			/*	
			if ( (*pRoundData)[CSSTAT_ROUNDS_WON] )
			{
				nTeamID = TEAM_CT;
			}
			else
			{
				nTeamID = TEAM_TERRORIST;
			}
			*/
		}

		//nGameType = g_pGameTypes ? g_pGameTypes->GetCurrentGameType() : -1;
		// HACK: Game type isn't useful to record in stats because for official servers
		// the map can tell us that. Game mode, however, isn't being captured. 
		// Game type is now game mode.
		nGameType = g_pGameTypes ? g_pGameTypes->GetCurrentGameMode() : -1;
		nRound = -1; // We'll get this data seperately, need to get it from the matchstats
		nReason = Invalid_Round_End_Reason; // We track this seperately since we normally get the round stats from the server but not all the pieces
	}

	uint32	nRoundTime;	
	int		nTeamID;
	int		nWinningTeamID;
	int		nWasKilled;
	int		nIsMVP;
	int		nDamageDealt;
	int		nMoneySpent;
	int		nStartingMoney;
	int		nRoundScore;
	int		nRevenges;
	int		nGameType;
	int		nRound;
	int		nReason;
	uint64  llExperimental;


	BEGIN_STAT_TABLE( "CSGORoundData" )
		REGISTER_STAT_NAMED( nRoundTime, "RoundTime" )
		REGISTER_STAT_NAMED( nTeamID, "TeamID" )
		REGISTER_STAT_NAMED( nWinningTeamID, "WinningTeamID" )
		REGISTER_STAT_NAMED( nWasKilled, "WasKilled" )
		REGISTER_STAT_NAMED( nIsMVP, "IsMvp" )
		REGISTER_STAT_NAMED( nDamageDealt, "DamageDealt" )
		REGISTER_STAT_NAMED( nMoneySpent, "MoneySpent" )
		REGISTER_STAT_NAMED( nStartingMoney, "StartingMoney" )
		REGISTER_STAT_NAMED( nRoundScore, "RoundScore" )
		REGISTER_STAT_NAMED( nRevenges, "Revenges" )
		REGISTER_STAT_NAMED( nGameType, "GameTypeID" )
		REGISTER_STAT_NAMED( nRound, "Round" )
		REGISTER_STAT_NAMED( nReason, "RoundEndReason" )
		REGISTER_STAT_NAMED( llExperimental, "Experimental" )
	END_STAT_TABLE()
};
#endif

class CCSClientGameStats : public CAutoGameSystem, public CGameEventListener
#if !defined( _GAMECONSOLE )
	, public IGameStatTracker
#endif
{
public:
	CCSClientGameStats();
	virtual void PostInit();
	virtual void LevelShutdownPreEntity();
	virtual void LevelInitPostEntity();

	int GetStatCount();

	void AddClientCSGOGameEvent( CSClientCsgoGameEventType_t eEvent, Vector const &pos, QAngle const &ang, uint64 ullData = 0ull, char const *szMapName = NULL, int16 nRound = CSCLIENTCSGOGAMEEVENTTYPE_AUTODETECT_INT16, int16 nRoundSecondsElapsed = CSCLIENTCSGOGAMEEVENTTYPE_AUTODETECT_INT16 );

	PlayerStatData_t GetStatById(int id, int nUserSlot );

	void		ResetAllStats( int nUserSlot );
	void		ResetAllStatsAndAchievements( void );
	void		ResetMatchStats( void );
	void		UpdateLastMatchStats( void );

	// [jhail] Reset the round stats 
	void		ResetRoundStats( void );

	// [jhail] Reset all leaderboard data on partnernet.  Does not work on retail builds/hardware.
	void		ResetLeaderboardStats( void );

	void		IncrementMatchmakingData( const StatsCollection_t &stats );
	void		UpdateMatchmakingData( void );
	void		ResetMatchmakingData( MatchmakingDataScope mmDataScope );

	const StatsCollection_t&	GetLifetimeStats( int nUserSlot );
	const StatsCollection_t&	GetMatchStats( int nUserSlot );

	// [jhail] Retrieve the per-round stats
	const StatsCollection_t&	GetRoundStats( int nUserSlot );

	bool		MsgFunc_PlayerStatsUpdate( const CCSUsrMsg_PlayerStatsUpdate &msg );

	bool ValidateTitleBlockVersion( struct TitleDataFieldsDescription_t const *pFields, class IPlayerLocal *pPlayerLocal, CSSyncStatValueDirection_t eOp, int titleBlockNo );

	bool SyncCSStatsToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSLoadoutsToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSMatchmakingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp );
	bool SyncCSRankingDataToTitleData( int iController, CSSyncStatValueDirection_t eOp );



	// [jhail] write stats to the leaderboard
	void WriteLeaderboardStats( void );

	// Public OGS functions and data
#if !defined( _GAMECONSOLE )
	
	virtual void SubmitGameStats( KeyValues *pKV )
	{
		int listCount = s_StatLists->Count();
		for( int i=0; i < listCount; ++i )
		{
			// Create a master key value that has stats everybody should share (map name, session ID, etc)
			(*s_StatLists)[i]->SendData(pKV);
			(*s_StatLists)[i]->Clear();
		}
	}

	virtual StatContainerList_t* GetStatContainerList( void )
	{
		return s_StatLists;
	}

	void	UploadRoundStats();
#endif

	CUserMessageBinder m_UMCMsgPlayerStatsUpdate;
	CUserMessageBinder m_UMCMsgXRankGet;
	CUserMessageBinder m_UMCMsgXRankUpd;

protected:
	void FireGameEvent( IGameEvent *event );

	void UpdateSteamStats();
	void RetrieveSteamStats();
	void UpdateStats( const StatsCollection_t &stats, int nUserSlot );
	void CalculateMatchFavoriteWeapons();
	
private:
	struct CsgoGameEvent_t
	{
		CSClientCsgoGameEventType_t m_eEvent;
		Vector m_pos;
		QAngle m_ang;
		uint64 m_ullData;
		CUtlSymbol m_symMap;
		int16 m_nRound;
		int16 m_numRoundSeconds;
		bool m_bRequireMoreReliableUpload;
	};
	CUtlVector< CsgoGameEvent_t >	m_arrClientCsgoGameEvents;
	StatsCollection_t				m_lifetimeStats[MAX_SPLITSCREEN_PLAYERS];
	StatsCollection_t				m_matchStats[MAX_SPLITSCREEN_PLAYERS];
	StatsCollection_t				m_roundStats[MAX_SPLITSCREEN_PLAYERS];

	// Value of the lifetime stats collection last time we updated steam with our current values.
	// we keep this to prevent spamming IPC calls for stats that haven't changed. 
	StatsCollection_t				m_lifetimeStatsLastUpload[MAX_SPLITSCREEN_PLAYERS];

	int							m_matchMaxPlayerCount;
	bool						m_bSteamStatsDownload;

	// Private OGS functions and data
#if !defined( _GAMECONSOLE )
	
	int							m_RoundEndReason;
	bool						m_bObjectiveAttempted;

	// A static list of all the stat containers, one for each data structure being tracked
	static StatContainerList_t * s_StatLists;
#endif
};

extern CCSClientGameStats g_CSClientGameStats;


#ifdef _X360

#define MAX_PROPS_CONTRIBSCORE		8
#define MAX_PROPS_KILLDEATH			6
#define MAX_PROPS_WINS				7
#define MAX_PROPS_STARS				5
#define MAX_PROPS_GAMESPLAYED		23

#define NUM_VIEW_PROPERTIES			5

class CAsyncLeaderboardWriteThread
{
public:
	CAsyncLeaderboardWriteThread();
	~CAsyncLeaderboardWriteThread();

	struct LeaderboardWriteData_t
	{
		int							userID;
		XUID						xuid;
		XSESSION_VIEW_PROPERTIES	viewProperties[NUM_VIEW_PROPERTIES];
		XUSER_PROPERTY				propertiesContribScore[MAX_PROPS_CONTRIBSCORE];
		XUSER_PROPERTY				propertiesKillDeath[MAX_PROPS_KILLDEATH];
		XUSER_PROPERTY				propertiesWins[MAX_PROPS_WINS];
		XUSER_PROPERTY				propertiesStars[MAX_PROPS_STARS];
		XUSER_PROPERTY				propertiesGamesPlayed[MAX_PROPS_GAMESPLAYED];
	};

	LeaderboardWriteData_t* CreateLeaderboardWriteData( void );

	static unsigned CallbackThreadProc( void *pvParam ) { reinterpret_cast<CAsyncLeaderboardWriteThread*>(pvParam)->ThreadProc(); return 0; }
	void ThreadProc( void );
	void QueueData( LeaderboardWriteData_t* pData );

protected:
	ThreadHandle_t		m_hThread;
	CThreadFastMutex	m_mutex;
	CUtlVector<LeaderboardWriteData_t*>	m_queue;
	HANDLE				m_hEvent;
};


void MsgFunc_ClientInfo( bf_read &msg );

#endif // _X360

#endif //CS_STEAMSTATS_H
