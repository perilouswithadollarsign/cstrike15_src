//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//


#ifndef _PLAYER_H_
#define _PLAYER_H_

class PlayerFriend;
class PlayerLocal;

#include "mm_framework.h"
#include "matchmaking/iplayer.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#include "playerrankingdata.h"
#include "mathlib/expressioncalculator.h"


//Players can be created several ways
//   -They are the local player and this is a reference to themselves
//   -They are created by the friends enumerator
//   -They are part of a party that a player joins
//   -They are part of a game that a player joins.

template < typename TBaseInterface >
class Player : public TBaseInterface
{
protected:
	Player() : m_xuid( 0 ), m_iController( XUSER_INDEX_NONE ), m_eOnlineState( IPlayer::STATE_OFFLINE )
	{
		memset( m_szName, 0, sizeof( m_szName ) );
		strcpy(m_szName, "EMPTY");
	}
	virtual ~Player() {}

public:
	// Updates connection info if needed.
	virtual void Update() = 0;

	// Deletes the player class instance
	virtual void Destroy() = 0;

	//
	// IPlayer implementation
	//
public:
	virtual XUID GetXUID() { return m_xuid; }
	virtual int GetPlayerIndex() { return m_iController; }

	virtual const char * GetName() { return m_szName; }

	virtual IPlayer::OnlineState_t GetOnlineState() { return m_eOnlineState; }

protected:
	XUID m_xuid;
	int m_iController;
	IPlayer::OnlineState_t m_eOnlineState;
	char m_szName[MAX_PLAYER_NAME_LENGTH];
};

class PlayerFriend : public Player< IPlayerFriend >
{
public:
	struct FriendInfo_t
	{
		char const *m_szName;
		wchar_t const *m_wszRichPresence;
		XNKID m_xSessionID;
		uint64 m_uiTitleID;
		uint32 m_uiGameServerIP;
		KeyValues *m_pGameDetails;
	};
public:
	explicit PlayerFriend( XUID xuid, FriendInfo_t const *pFriendInfo = NULL );

	//
	// IPlayerFriend implementation
	//
public:
	virtual wchar_t const * GetRichPresence();

	virtual KeyValues *GetGameDetails();
	virtual KeyValues *GetPublishedPresence();

	virtual bool IsJoinable();
	virtual void Join();
	virtual uint64 GetTitleID();
	virtual uint32 GetGameServerIP();

	//
	// Player<IPlayerFriend> implementation
	//
public:
	virtual void Update();
	virtual void Destroy();

public:
	void SetFriendMark( unsigned maskSetting );
	unsigned GetFriendMark( );

	void SetIsStale( bool bStale );
	bool GetIsStale();

	void UpdateFriendInfo( FriendInfo_t const *pFriendInfo );
	bool IsUpdatingInfo();

public:
	void StartSearchForSessionInfo();
	void StartSearchForSessionInfoImpl();
	void AbortSearch();

protected:
	unsigned m_uFriendMark;
	bool m_bIsStale;
	wchar_t m_wszRichPresence[MAX_RICHPRESENCE_SIZE];

	uint64 m_uiTitleID;
	uint32 m_uiGameServerIP;
	XNKID m_xSessionID;
	XSESSION_INFO m_GameSessionInfo;
	KeyValues *m_pDetails;
	KeyValues *m_pPublishedPresence;

	enum SearchState_t
	{
		SEARCH_NONE,
		SEARCH_QUEUED,
#ifdef _X360
		SEARCH_XNKID,
		SEARCH_QOS,
#elif !defined( NO_STEAM )
		SEARCH_WAIT_LOBBY_DATA,
#endif
		SEARCH_COMPLETED
	};
	SearchState_t m_eSearchState;
#ifdef _X360
	XSESSION_INFO m_xsiSearchState;
	XNADDR const *m_pQOS_xnaddr;
	XNKID const  *m_pQOS_xnkid;
	XNKEY const  *m_pQOS_xnkey;
	XNQOS		*m_XNQOS;
	XOVERLAPPED m_SessionSearchOverlapped;
	
	CUtlBuffer m_bufSessionSearchResults;
	XSESSION_SEARCHRESULT_HEADER * GetXSearchResults() { return ( XSESSION_SEARCHRESULT_HEADER * ) m_bufSessionSearchResults.Base(); }

	void Live_Update_SearchXNKID();
	void Live_Update_Search_QOS();
#elif !defined( NO_STEAM )
	STEAM_CALLBACK_MANUAL( PlayerFriend, Steam_OnLobbyDataUpdate, LobbyDataUpdate_t, m_CallbackOnLobbyDataUpdate );
#endif
};

class PlayerLocal : public Player< IPlayerLocal >
{
public:
	explicit PlayerLocal( int iController );
	~PlayerLocal();

public:
	void RecomputeXUID( char const *szNetwork );
	void DetectOnlineState();

	//
	// IPlayerLocal implementation
	//
public:
	virtual const UserProfileData& GetPlayerProfileData();

	virtual MatchmakingData * GetPlayerMatchmakingData( void );
	virtual void UpdatePlayerMatchmakingData( int mmDataType );
	virtual void ResetPlayerMatchmakingData( int mmDataScope );

	virtual const void * GetPlayerTitleData( int iTitleDataIndex );
	virtual void UpdatePlayerTitleData( TitleDataFieldsDescription_t const *fdKey, const void *pvNewTitleData, int numNewBytes );

	virtual void GetLeaderboardData( KeyValues *pLeaderboardInfo );
	virtual void UpdateLeaderboardData( KeyValues *pLeaderboardInfo );

	virtual void GetAwardsData( KeyValues *pAwardsData );
	virtual void UpdateAwardsData( KeyValues *pAwardsData );

	virtual void SetNeedsSave( void );

#if defined ( _X360 )
	virtual bool IsTitleDataValid() { return m_bIsTitleDataValid; };
	virtual bool IsTitleDataBlockValid( int blockId );
	virtual void SetIsTitleDataValid( bool isValid ) { m_bIsTitleDataValid = isValid; }
	virtual bool IsFreshPlayerProfile( void ) { return m_bIsFreshPlayerProfile; }
	virtual void ClearBufTitleData( void );
#endif

	//
	// Player<IPlayerLocal> implementation
	//
public:
	virtual void Update();
	virtual void Destroy();

public:
	XUSER_SIGNIN_STATE GetAssumedSigninState();
	void LoadPlayerProfileData();
	void LoadTitleData();
	void SetTitleDataWriteTime( float flTime );
	void WriteTitleData();
	bool IsTitleDataStorageConnected( void );
	void OnLeaderboardRequestFinished( KeyValues *pLeaderboardData );

	void SetFlag_AwaitingTitleData() { m_uiPlayerFlags |= PLAYER_INVITE_AWAITING_TITLEDATA; }
#if defined ( _X360 )
	bool m_bIsTitleDataValid;
	bool m_bIsFreshPlayerProfile;
#endif

protected:
	//stats
	XUSER_SIGNIN_STATE m_eLoadedTitleData;
	float m_flLastSave;

	enum Flags_t
	{
		PLAYER_INVITE_AWAITING_TITLEDATA = ( 1 << 0 )
	};
	uint32 m_uiPlayerFlags;

	enum Consts_t
	{
		TITLE_DATA_COUNT_X360 = 3,	// number of Xbox LIVE - backed title data buffers
		TITLE_DATA_COUNT = 3,		// number of valid title data buffers
	};

	UserProfileData m_ProfileData;
	MatchmakingData m_MatchmakingData;
	CUtlVector<int> m_arrAchievementsEarned, m_arrAvatarAwardsEarned;
	char m_bufTitleData[TITLE_DATA_COUNT][XPROFILE_SETTING_MAX_SIZE];
	bool m_bSaveTitleData[TITLE_DATA_COUNT];
#if defined ( _X360 )
		bool m_bIsTitleDataBlockValid[TITLE_DATA_COUNT];
#endif

	KeyValues *m_pLeaderboardData;
	KeyValues::AutoDelete m_autodelete_pLeaderboardData;

#ifdef _X360
	struct XPendingAsyncAward_t
	{
		float m_flStartTimestamp;
		XOVERLAPPED m_xOverlapped;
		PlayerLocal *m_pLocalPlayer;
		enum Type_t
		{
			TYPE_ACHIEVEMENT,
			TYPE_AVATAR_AWARD
		};
		Type_t m_eType;
		union
		{
			TitleAchievementsDescription_t const *m_pAchievementDesc;
			TitleAvatarAwardsDescription_t const *m_pAvatarAwardDesc;
		};
		union
		{
			XUSER_ACHIEVEMENT m_xAchievement;
			XUSER_AVATARASSET m_xAvatarAsset;
		};
	};
	static CUtlVector< XPendingAsyncAward_t * > s_arrPendingAsyncAwards;
	void UpdatePendingAwardsState();
#elif !defined( NO_STEAM )
	void UpdatePlayersSteamLogon();

	STEAM_CALLBACK_MANUAL( PlayerLocal, Steam_OnUserStatsReceived, UserStatsReceived_t, m_CallbackOnUserStatsReceived );
	STEAM_CALLBACK_MANUAL( PlayerLocal, Steam_OnPersonaStateChange, PersonaStateChange_t, m_CallbackOnPersonaStateChange );
	STEAM_CALLBACK_MANUAL( PlayerLocal, Steam_OnServersConnected, SteamServersConnected_t, m_CallbackOnServersConnected );
	STEAM_CALLBACK_MANUAL( PlayerLocal, Steam_OnServersDisconnected, SteamServersDisconnected_t, m_CallbackOnServersDisconnected );
#endif

	void EvaluateAwardsStateBasedOnStats();
	void LoadGuestsTitleData();
	void OnProfileTitleDataLoaded( int iErrorCode );

protected:

};

//
// Players XWrite opportunities
//

// XWrite opportunities
enum MM_XWriteOpportunity
{
	MMXWO_NONE = 0,
	MMXWO_SETTINGS,
	MMXWO_SESSION_STARTED,
	MMXWO_CHECKPOINT,
	MMXWO_SESSION_FINISHED
};

void SignalXWriteOpportunity( MM_XWriteOpportunity eXWO );


#endif
