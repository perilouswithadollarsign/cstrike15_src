//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//


#ifndef _PLAYERMANAGER_H_
#define _PLAYERMANAGER_H_


class PlayerManager;

#include <list>
#include "utlvector.h"
#include "utlmap.h"

#ifndef SWDS

#include "player.h"


class PlayerManager: public IPlayerManager, public IMatchEventsSink
{
public :
	PlayerManager();
	virtual ~PlayerManager();

	//IPlayerManager
public:
	//
	// EnableServersUpdate
	//	controls whether friends data is being updated in the background
	//
	virtual void EnableFriendsUpdate( bool bEnable );

	//
	// GetLocalPlayer
	//	returns a local player interface for a given controller index
	//
	virtual IPlayerLocal * GetLocalPlayer( int iController );

	//
	// GetNumFriends
	//	returns number of friends discovered and for which data is available
	//
	virtual int GetNumFriends();

	//
	// GetFriend
	//	returns player interface to the given friend or NULL if friend not found or not available
	//
	virtual IPlayerFriend * GetFriendByIndex( int index );
	virtual IPlayerFriend * GetFriendByXUID( XUID xuid );

	//
	// FindPlayer
	//	returns player interface by player's XUID or NULL if friend not found or not available
	//
	virtual IPlayer * FindPlayer( XUID xuid );

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

public:
	void OnGameUsersChanged();
	void Update();
	void OnLocalPlayerDisconnectedFromLive( int iCtrlr );
	void RecomputePlayerXUIDs( char const *szNetwork );

	PlayerFriend *FindPlayerFriend( XUID xuid );
	PlayerLocal *FindPlayerLocal( XUID xuid );
	void RequestStoreStats();

protected:
	void MarkOldFriends();
	void RemoveOldFriends();
	void OnSigninChange( KeyValues *pEvent );
	void OnLostConnectionToConsoleNetwork();
#if defined( _PS3 ) && !defined( NO_STEAM )
	STEAM_CALLBACK( PlayerManager, Steam_OnPS3PSNStatusChange, PS3PSNStatusChange_t, m_CallbackOnPS3PSNStatusChange );
#endif

private:
	void CreateFriendEnumeration( int iCtrlr );
	void CreateLanSearch();
	void UpdateLanSearch();
	void ExecuteStoreStatsRequest();

	// Players instances
	PlayerLocal * mLocalPlayer[ XUSER_MAX_COUNT ]; // local Players are cached off here for convienience.
	CUtlVector< PlayerFriend * > mFriendsList;

	bool m_bUpdateEnabled;		// whether data should be auto-updated
	float m_flNextUpdateTime;	// when next update cycle should occur

	struct SFriendSearchData
	{
		SFriendSearchData() { memset( this, 0, sizeof( *this ) ); }

		bool mSearchInProgress;
		void * mFriendBuffer;
		int mFriendBufferSize;
		int mFriendsStartIndex;
#ifdef _X360
		HANDLE mFriendEnumHandle;
		XOVERLAPPED mFriendsOverlapped;
#endif
		XUID mXuid;
	};
	SFriendSearchData m_searchData[ XUSER_MAX_COUNT ];

	struct SLanSearchData_t
	{
		SLanSearchData_t() { memset( this, 0, sizeof( *this ) ); }

		bool m_bSearchInProgress;
		float m_flStartTime;
		float m_flLastBroadcastTime;
	};
	SLanSearchData_t m_lanSearchData;

	int m_searchesPending; // Number of searches in progress
	bool m_bRequestStoreStats;
};

#else // SWDS

class PlayerManager: public IPlayerManager, public IMatchEventsSink
{
public:
	// SWDS declaration is intentionally left stripped
	virtual void Update() = 0;
};

#endif

extern class PlayerManager *g_pPlayerManager;

#endif // _PLAYERMANAGER_H_