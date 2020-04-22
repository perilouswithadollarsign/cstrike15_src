#ifndef _ISERVERMANAGER_H_
#define _ISERVERMANAGER_H_

class IServer;
class IServerManager;

#include "imatchsystem.h"

abstract_class IMatchServer
{
public:
	//
	// GetOnlineId
	//	returns server online id to store as reference
	//
	virtual XUID GetOnlineId() = 0;

	//
	// GetGameDetails
	//	returns server game details
	//
	virtual KeyValues *GetGameDetails() = 0;

	//
	// IsJoinable and Join
	//	returns whether server is joinable and initiates join to the server
	//
	virtual bool IsJoinable() = 0;
	virtual void Join() = 0;
};

abstract_class IServerManager
{
public:
	//
	// EnableServersUpdate
	//	controls whether server data is being updated in the background
	//
	virtual void EnableServersUpdate( bool bEnable ) = 0;

	//
	// GetNumServers
	//	returns number of servers discovered and for which data is available
	//
	virtual int GetNumServers() = 0;

	//
	// GetServerByIndex / GetServerByOnlineId
	//	returns server interface to the given server or NULL if server not found or not available
	//
	virtual IMatchServer* GetServerByIndex( int iServerIdx ) = 0;
	virtual IMatchServer* GetServerByOnlineId( XUID xuidServerOnline ) = 0;
};


#endif // _ISERVERMANAGER_H_
