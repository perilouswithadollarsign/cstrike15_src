//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _SERVERMANAGER_H_
#define _SERVERMANAGER_H_

#include "utlvector.h"
#include "utlmap.h"


class CServer :
	public IMatchServer
{
public:
	CServer();
	virtual ~CServer();

	//
	//	IMatchServer implementation
	//
public:
	//
	// GetOnlineId
	//	returns server online id to store as reference
	//
	virtual XUID GetOnlineId();

	//
	// GetGameDetails
	//	returns server game details
	//
	virtual KeyValues *GetGameDetails();

	//
	// IsJoinable and Join
	//	returns whether server is joinable and initiates join to the server
	//
	virtual bool IsJoinable();
	virtual void Join();

public:
	float m_flLastRefresh;
	XUID m_xuid;
	KeyValues *m_pGameDetails;
#if !defined( NO_STEAM ) && !defined( SWDS )
	servernetadr_t m_netAdr;
#endif
};

class CServerManager :
	public IServerManager,
#if !defined( NO_STEAM ) && !defined( SWDS )
	public ISteamMatchmakingServerListResponse,
#endif
	public IMatchEventsSink
{
public :
	CServerManager();
	virtual ~CServerManager();

	//
	//	IServerManager implementation
	//
public:
	//
	// EnableServersUpdate
	//	controls whether server data is being updated in the background
	//
	virtual void EnableServersUpdate( bool bEnable );

	//
	// GetNumServers
	//	returns number of servers discovered and for which data is available
	//
	virtual int GetNumServers();

	//
	// GetServerByIndex / GetServerByOnlineId
	//	returns server interface to the given server or NULL if server not found or not available
	//
	virtual IMatchServer* GetServerByIndex( int iServerIdx );
	virtual IMatchServer* GetServerByOnlineId( XUID xuidServerOnline );

	// IMatchEventsSink
public:
	virtual void OnEvent( KeyValues *pEvent );

#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
protected:
	HServerListRequest m_hRequest;
public:
	// ISteamMatchmakingServerListResponse implementation
	virtual void ServerResponded( HServerListRequest hReq, int iServer );
	virtual void ServerFailedToRespond( HServerListRequest hReq, int iServer ) {}
	virtual void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response );
#endif

	//
	// Interface for match system
	//
public:
	void Update();

protected:
	void MarkOldServersAndBeginSearch();
	void RemoveOldServers();

	void UpdateLanSearch();
	void OnGroupFetched();
	void OnAllGroupsFetched();
	void RequestPingingDetails();
	void UpdateRequestingDetails();
	void OnAllDataFetched();

	bool StartFetchingGroupServersData();
	bool FetchGroupServers();

	CServer * GetServerRecordByOnlineId( CUtlVector< CServer * > &arr, XUID xuidServerOnline );

	//
	// Internal data
protected:
	bool m_bUpdateEnabled;		// whether data should be auto-updated
	float m_flNextUpdateTime;	// when next update cycle should occur
	float m_flNextServerUpdateTime; 

	// list of servers
	CUtlVector< CServer * > m_Servers;
	CUtlVector< CServer * > m_ServersPinging;

	enum State
	{
		STATE_IDLE,
		STATE_LAN_SEARCH,
		STATE_GROUP_SEARCH,
#if !defined( _X360 ) && !defined( NO_STEAM ) && !defined( SWDS )
		STATE_FETCHING_SERVERS,
#endif
		STATE_GROUP_FETCHED,
		STATE_REQUESTING_DETAILS,
	};
	State m_eState;
	
	struct SLanSearchData_t
	{
		SLanSearchData_t() { memset( this, 0, sizeof( *this ) ); }

		float m_flStartTime;
		float m_flLastBroadcastTime;
	};
	SLanSearchData_t m_lanSearchData;

	struct SGroupSearchData_t
	{
		SGroupSearchData_t() : m_idxSearchGroupId( 0 ) {}
		void Reset() { m_idxSearchGroupId = 0; m_UserGroupAccountIDs.RemoveAll(); }

		int m_idxSearchGroupId;
		CUtlVector< uint32 > m_UserGroupAccountIDs;	// list of user groups
	};
	SGroupSearchData_t m_groupSearchData;
};

extern class CServerManager *g_pServerManager;

#endif // _SERVERMANAGER_H_
