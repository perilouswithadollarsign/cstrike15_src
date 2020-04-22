#if defined( INCLUDE_SCALEFORM )
//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LEADERBOARDSDIALOG_SCALEFORM_H
#define LEADERBOARDSDIALOG_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"
#include "utlvector.h"

#if !defined( NO_STEAM )
#include "steam/isteamuserstats.h"
#include "steam/steam_api.h"
#endif

enum eLeaderboardFiltersType
{
	eLBFilter_Overall		= 0,
	eLBFilter_Me			= 1,
	eLBFilter_Friends		= 2
};

class CCreateLeaderboardsDialogScaleform : public ScaleformFlashInterface
{
protected:
	static CCreateLeaderboardsDialogScaleform* m_pInstance;

	CCreateLeaderboardsDialogScaleform();

public:
	static void LoadDialog( void );
	static void UnloadDialog( void );
	static void UpdateDialog( void );

	void OnOk( SCALEFORM_CALLBACK_ARGS_DECL );
	void SetQuery( SCALEFORM_CALLBACK_ARGS_DECL );
	
	void Query_NumResults( SCALEFORM_CALLBACK_ARGS_DECL );		// Query the number of results for this query
	void Query_GetCurrentPlayerRow( SCALEFORM_CALLBACK_ARGS_DECL );	// Query the number of results for this query
	void QueryRow_GamerTag( SCALEFORM_CALLBACK_ARGS_DECL );		// Retrieve the gamertag of the user at a specified row
	void QueryRow_ColumnValue( SCALEFORM_CALLBACK_ARGS_DECL );	// Retrieve a single value from the row we retrieved
	void QueryRow_ColumnRatio( SCALEFORM_CALLBACK_ARGS_DECL );	// Retrieve the ratio between two column values we retrieved

	void DisplayUserInfo( SCALEFORM_CALLBACK_ARGS_DECL );		// Show the gamer card for the row specified

protected:
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );
	virtual void PostUnloadFlash( void );
	virtual void FlashLoaded( void );

	virtual void Tick( void );

	void Show( void );
	void Hide( void );

	void QueryUpdate( void );
	void CheckForQueryResults( void );

#if !defined( NO_STEAM )
	SteamLeaderboard_t GetLeaderboardHandle( const char* szLeaderboardName );
	void SetLeaderboardHandle( const char* szLeaderboardName, SteamLeaderboard_t hLeaderboard );

	CCallResult<CCreateLeaderboardsDialogScaleform, LeaderboardFindResult_t> m_SteamCallResultFindLeaderboard;
	void Steam_OnFindLeaderboard( LeaderboardFindResult_t *pFindLeaderboardResult, bool bIOFailure );

	CCallResult< CCreateLeaderboardsDialogScaleform, LeaderboardScoresDownloaded_t > m_SteamCallbackOnLeaderboardScoresDownloaded;
	void Steam_OnLeaderboardScoresDownloaded( LeaderboardScoresDownloaded_t *p, bool bError );

	// Extract data from the given payload on the active leaderboard (m_currentLeaderboardName) and return it as uint64
	uint64	ExtractPayloadDataByColumnID( int *pData, int columnId );

	void	QueryLeaderboard();

#endif

private:
	int									m_iPlayerSlot;
	XUID								m_PlayerXUID;

	// Platform-specific stats storage
#ifdef _X360
	XUSER_STATS_SPEC					m_statsSpec;
	XUSER_STATS_READ_RESULTS*			m_pResultsBuffer;
	CUtlVector<XUSER_STATS_ROW*>		m_pResults;
	
	XONLINE_FRIEND*						m_pFriends;
	XUSER_STATS_READ_RESULTS*			m_pFriendsResult[MAX_FRIENDS+1];
#endif

#if !defined( NO_STEAM )
	// Map from name of board to Steam handle
	CUtlMap< const char*, SteamLeaderboard_t >	m_LeaderboardHandles;

	const char*							m_currentLeaderboardName;
	SteamLeaderboard_t					m_currentLeaderboardHandle;

	LeaderboardScoresDownloaded_t		m_cachedLeaderboardScores;
	KeyValues							*m_pLeaderboardDescription;

	// NOTE: If the number of payload entries ever exceeds this number, you'll have to manually increase it
	static const int					kMaxPayloadEntries = 16;

	int									m_payloadSizes[kMaxPayloadEntries];	// Extracted from the payload format data in the KV description
#endif

	AsyncHandle_t						m_hAsyncQuery;
	bool								m_bCheckForQueryResults;
	bool								m_bResultsValid;
	int									m_iTotalViewRows;
	
	int									m_iNumFriends;
	int									m_iNextFriend;	// for querying our friends list for stats
	bool								m_bEnumeratingFriends;

	float								m_fQueryDelayTime;

	eLeaderboardFiltersType				m_currentFilterType;
	int									m_startingRowIndex;
	int									m_rowsPerPage;
};

//=============================================================================
// HPE_END
//=============================================================================

#endif // LEADERBOARDSDIALOG_SCALEFORM_H
#endif // include scaleform
